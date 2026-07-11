const Visualizer = (() => {
  let svg, g;
  let zoomBehavior;
  const duration = 400;

  function getColors() {
    const style = getComputedStyle(document.documentElement);
    return {
      nodeDefault: "#94a3b8",
      nodePrefix: "#22c55e",
      nodeHighlight: "#f59e0b",
      linkDefault: style.getPropertyValue("--border").trim() || "#cbd5e1",
      linkHighlight: "#f59e0b",
      textDefault: style.getPropertyValue("--text-primary").trim() || "#1e293b",
      edgeLabel: style.getPropertyValue("--text-secondary").trim() || "#64748b",
      backEdge: "#ef4444",
    };
  }

  function setup(containerSelector) {
    const container = d3.select(containerSelector);
    const { width, height } = container.node().getBoundingClientRect();

    svg = container
      .append("svg")
      .attr("width", "100%")
      .attr("height", "100%")
      .attr("viewBox", `0 0 ${width} ${height}`);

    const defs = svg.append("defs");
    defs.append("marker")
      .attr("id", "back-arrow")
      .attr("viewBox", "0 0 10 10")
      .attr("refX", 8).attr("refY", 5)
      .attr("markerWidth", 6).attr("markerHeight", 6)
      .attr("orient", "auto-start-reverse")
      .append("path").attr("d", "M 0 0 L 10 5 L 0 10 z").attr("fill", "#ef4444");

    g = svg.append("g").attr("transform", `translate(${width / 2}, 40)`);

    zoomBehavior = d3.zoom().scaleExtent([0.1, 4])
      .on("zoom", (event) => g.attr("transform", event.transform));

    svg.call(zoomBehavior);
    svg.call(zoomBehavior.transform, d3.zoomIdentity.translate(width / 2, 40));
  }

  // ---------- Tree-based render (Standard + Compressed) ----------

  function renderTree(data, highlightPath) {
    if (!data || !g) return;

    // Clear PATRICIA graph elements
    g.selectAll(".p-node").remove();
    g.selectAll(".p-edge").remove();

    const colors = getColors();
    const root = d3.hierarchy(data, (d) => d.children);

    const maxLabelLen = Math.max(
      1, ...root.descendants().map((d) => (d.data.bit === "root" ? 1 : d.data.bit.length))
    );
    const hSpacing = Math.max(120, maxLabelLen * 8 + 60);
    const vSpacing = Math.max(80, maxLabelLen * 5 + 40);

    d3.tree().nodeSize([hSpacing, vSpacing])(root);

    const highlightSet = new Set(highlightPath || []);

    // --- Links ---
    const links = g.selectAll(".link").data(root.links(), (d) => nodeId(d.target.data));
    links.exit().transition().duration(duration).style("opacity", 0).remove();

    const linkEnter = links.enter().append("g").attr("class", "link").style("opacity", 0);
    linkEnter.append("path").attr("fill", "none").attr("stroke-width", 2);
    linkEnter.append("text").attr("class", "edge-label")
      .attr("text-anchor", "middle").attr("font-size", "10px")
      .attr("font-family", "'SF Mono', 'Fira Code', monospace");

    const linkMerge = linkEnter.merge(links);
    linkMerge.transition().duration(duration).style("opacity", 1);

    linkMerge.select("path").transition().duration(duration)
      .attr("d", (d) => `M${d.source.x},${d.source.y} C${d.source.x},${(d.source.y + d.target.y) / 2} ${d.target.x},${(d.source.y + d.target.y) / 2} ${d.target.x},${d.target.y}`)
      .attr("stroke", (d) => highlightSet.has(nodeId(d.target.data)) ? colors.linkHighlight : colors.linkDefault)
      .attr("stroke-width", (d) => highlightSet.has(nodeId(d.target.data)) ? 3 : 2);

    linkMerge.select(".edge-label")
      .attr("fill", colors.edgeLabel)
      .transition().duration(duration)
      .attr("x", (d) => { const mx = (d.source.x + d.target.x) / 2; return mx + (d.target.x >= d.source.x ? -10 : 10); })
      .attr("y", (d) => (d.source.y + d.target.y) / 2)
      .attr("transform", (d) => {
        const mx = (d.source.x + d.target.x) / 2 + (d.target.x >= d.source.x ? -10 : 10);
        const my = (d.source.y + d.target.y) / 2;
        let angle = Math.atan2(d.target.y - d.source.y, d.target.x - d.source.x) * (180 / Math.PI);
        if (angle > 90) angle -= 180; if (angle < -90) angle += 180;
        return `rotate(${angle},${mx},${my})`;
      })
      .text((d) => d.target.data.bit);

    // --- Nodes ---
    const nodes = g.selectAll(".node").data(root.descendants(), (d) => nodeId(d.data));
    nodes.exit().transition().duration(duration).style("opacity", 0).remove();

    const nodeEnter = nodes.enter().append("g").attr("class", "node").style("opacity", 0);
    nodeEnter.append("circle");
    nodeEnter.append("text").attr("class", "node-label")
      .attr("text-anchor", "middle").attr("font-size", "10px")
      .attr("font-weight", "600").attr("font-family", "'SF Mono', 'Fira Code', monospace");
    nodeEnter.append("text").attr("class", "node-sublabel")
      .attr("text-anchor", "middle").attr("font-size", "9px")
      .attr("font-family", "'SF Mono', 'Fira Code', monospace");

    const nodeMerge = nodeEnter.merge(nodes);
    nodeMerge.transition().duration(duration)
      .attr("transform", (d) => `translate(${d.x},${d.y})`).style("opacity", 1);

    nodeMerge.select("circle").transition().duration(duration)
      .attr("r", (d) => d.data.bit === "root" ? 14 : (d.data.prefix ? 10 : 6))
      .attr("fill", (d) => {
        if (d.data.bit === "root") return "#3b82f6";
        if (highlightSet.has(nodeId(d.data))) return colors.nodeHighlight;
        return d.data.prefix ? colors.nodePrefix : colors.nodeDefault;
      })
      .attr("stroke", (d) => {
        if (d.data.bit === "root") return "#2563eb";
        if (highlightSet.has(nodeId(d.data))) return "#d97706";
        return d.data.prefix ? "#16a34a" : "#64748b";
      })
      .attr("stroke-width", (d) => d.data.bit === "root" ? 3 : 2);

    nodeMerge.select(".node-label").attr("fill", colors.textDefault)
      .attr("dy", (d) => d.data.prefix ? 24 : -14)
      .text((d) => d.data.bit === "root" ? "root" : (d.data.prefix || ""));
    nodeMerge.select(".node-sublabel").text("");
  }

  // ---------- Graph-based render (PATRICIA) ----------

  function renderPatricia(graphData, highlightPath) {
    if (!graphData || !g) return;

    // Clear tree elements
    g.selectAll(".link").remove();
    g.selectAll(".node").remove();

    const colors = getColors();
    const { nodes: nodeList, edges } = graphData;
    if (!nodeList || !edges) return;

    const highlightSet = new Set(highlightPath || []);

    // Layout: position by depth, spread horizontally within each level
    const levels = {};
    nodeList.forEach((n) => {
      const d = n.depth || 0;
      if (!levels[d]) levels[d] = [];
      levels[d].push(n);
    });

    const hSpacing = 140;
    const vSpacing = 100;
    const positions = {};

    Object.keys(levels).sort((a, b) => a - b).forEach((depth) => {
      const nodesAtLevel = levels[depth];
      const totalWidth = (nodesAtLevel.length - 1) * hSpacing;
      nodesAtLevel.forEach((n, i) => {
        positions[n.id] = {
          x: -totalWidth / 2 + i * hSpacing,
          y: parseInt(depth) * vSpacing,
        };
      });
    });

    // Separate forward and back edges
    const forwardEdges = edges.filter((e) => e.type === "forward");
    const backEdges = edges.filter((e) => e.type === "back");

    // --- Forward edges ---
    const fEdges = g.selectAll(".p-edge.forward").data(forwardEdges, (d) => `f-${d.from}-${d.to}-${d.bit}`);
    fEdges.exit().transition().duration(duration).style("opacity", 0).remove();

    const fEnter = fEdges.enter().append("g").attr("class", "p-edge forward").style("opacity", 0);
    fEnter.append("path").attr("fill", "none").attr("stroke-width", 2);
    fEnter.append("text").attr("class", "edge-label")
      .attr("text-anchor", "middle").attr("font-size", "10px")
      .attr("font-family", "'SF Mono', 'Fira Code', monospace");

    const fMerge = fEnter.merge(fEdges);
    fMerge.transition().duration(duration).style("opacity", 1);

    fMerge.select("path").transition().duration(duration)
      .attr("d", (d) => {
        const f = positions[d.from], t = positions[d.to];
        if (!f || !t) return "";
        return `M${f.x},${f.y} C${f.x},${(f.y + t.y) / 2} ${t.x},${(f.y + t.y) / 2} ${t.x},${t.y}`;
      })
      .attr("stroke", colors.linkDefault).attr("stroke-width", 2);

    fMerge.select(".edge-label").attr("fill", colors.edgeLabel)
      .transition().duration(duration)
      .attr("x", (d) => {
        const f = positions[d.from], t = positions[d.to];
        if (!f || !t) return 0;
        return (f.x + t.x) / 2 + (t.x >= f.x ? -10 : 10);
      })
      .attr("y", (d) => {
        const f = positions[d.from], t = positions[d.to];
        return f && t ? (f.y + t.y) / 2 : 0;
      })
      .text((d) => d.bit);

    // --- Back edges ---
    const bEdges = g.selectAll(".p-edge.back").data(backEdges, (d) => `b-${d.from}-${d.to}-${d.bit}`);
    bEdges.exit().transition().duration(duration).style("opacity", 0).remove();

    const bEnter = bEdges.enter().append("g").attr("class", "p-edge back").style("opacity", 0);
    bEnter.append("path").attr("fill", "none")
      .attr("stroke", colors.backEdge).attr("stroke-width", 1.5)
      .attr("stroke-dasharray", "5,4").attr("marker-end", "url(#back-arrow)");
    bEnter.append("text").attr("class", "back-edge-label")
      .attr("text-anchor", "middle").attr("font-size", "9px")
      .attr("font-family", "'SF Mono', 'Fira Code', monospace")
      .attr("fill", colors.backEdge);

    const bMerge = bEnter.merge(bEdges);
    bMerge.transition().duration(duration).style("opacity", 0.7);

    bMerge.select("path").transition().duration(duration)
      .attr("d", (d) => {
        const f = positions[d.from], t = positions[d.to];
        if (!f || !t) return "";
        if (d.from === d.to) {
          return `M${f.x + 12},${f.y - 5} C${f.x + 45},${f.y - 35} ${f.x + 45},${f.y + 25} ${f.x + 12},${f.y + 5}`;
        }
        const dx = t.x - f.x, dy = t.y - f.y;
        const dist = Math.sqrt(dx * dx + dy * dy);
        const bulge = Math.max(40, dist * 0.5);
        const side = d.bit === "0" ? -1 : 1;
        const mx = (f.x + t.x) / 2 + side * bulge;
        const my = (f.y + t.y) / 2;
        return `M${f.x},${f.y} Q${mx},${my} ${t.x},${t.y}`;
      });

    bMerge.select(".back-edge-label").transition().duration(duration)
      .attr("x", (d) => {
        const f = positions[d.from], t = positions[d.to];
        if (!f || !t) return 0;
        if (d.from === d.to) return f.x + 50;
        const dist = Math.sqrt((t.x - f.x) ** 2 + (t.y - f.y) ** 2);
        const side = d.bit === "0" ? -1 : 1;
        return (f.x + t.x) / 2 + side * Math.max(40, dist * 0.5);
      })
      .attr("y", (d) => {
        const f = positions[d.from], t = positions[d.to];
        if (!f || !t) return 0;
        if (d.from === d.to) return f.y - 8;
        return (f.y + t.y) / 2 - 6;
      })
      .text((d) => d.bit);

    // --- Nodes ---
    const pNodes = g.selectAll(".p-node").data(nodeList, (d) => `pn-${d.id}`);
    pNodes.exit().transition().duration(duration).style("opacity", 0).remove();

    const pEnter = pNodes.enter().append("g").attr("class", "p-node").style("opacity", 0);
    pEnter.append("circle");
    pEnter.append("text").attr("class", "node-label")
      .attr("text-anchor", "middle").attr("font-size", "10px")
      .attr("font-weight", "600").attr("font-family", "'SF Mono', 'Fira Code', monospace");
    pEnter.append("text").attr("class", "node-sublabel")
      .attr("text-anchor", "middle").attr("font-size", "9px")
      .attr("font-family", "'SF Mono', 'Fira Code', monospace");

    const pMerge = pEnter.merge(pNodes);
    pMerge.transition().duration(duration)
      .attr("transform", (d) => {
        const p = positions[d.id];
        return p ? `translate(${p.x},${p.y})` : "";
      })
      .style("opacity", 1);

    pMerge.select("circle").transition().duration(duration)
      .attr("r", (d) => d.is_root ? 14 : (d.prefix ? 10 : 6))
      .attr("fill", (d) => {
        if (d.is_root) return "#3b82f6";
        if (highlightSet.has(d.id)) return colors.nodeHighlight;
        return d.prefix ? colors.nodePrefix : colors.nodeDefault;
      })
      .attr("stroke", (d) => {
        if (d.is_root) return "#2563eb";
        if (highlightSet.has(d.id)) return "#d97706";
        return d.prefix ? "#16a34a" : "#64748b";
      })
      .attr("stroke-width", (d) => d.is_root ? 3 : 2);

    pMerge.select(".node-label").attr("fill", colors.textDefault).attr("dy", -16)
      .text((d) => d.is_root ? "root" : `bit ${d.bit_index}`);

    pMerge.select(".node-sublabel").attr("fill", colors.nodePrefix).attr("dy", 24)
      .text((d) => d.prefix || "");
  }

  // ---------- Public API ----------

  function render(data, highlightPath, opts) {
    const { isPatricia } = opts || {};

    if (isPatricia) {
      renderPatricia(data, highlightPath);
    } else {
      renderTree(data, highlightPath);
    }
  }

  function nodeId(data) {
    return data._path;
  }

  function buildHighlightPath(data, ip, matchedPrefix) {
    if (!data || !ip || !matchedPrefix) return [];

    const prefixLen = parseInt(matchedPrefix.split("/")[1], 10);
    const parts = ip.split(".");
    if (parts.length !== 4) return [];

    let ipNum = 0;
    for (let i = 0; i < 4; i++) {
      const v = parseInt(parts[i], 10);
      if (isNaN(v) || v < 0 || v > 255) return [];
      ipNum = (ipNum * 256 + v) >>> 0;
    }

    // For tree modes (standard/compressed)
    if (data.bit !== undefined) {
      const path = [nodeId(data)];
      let node = data;
      let bitsConsumed = 0;
      while (bitsConsumed < prefixLen && node) {
        const bit = (ipNum >>> (31 - bitsConsumed)) & 1;
        const child = node.children
          ? node.children.find((c) => c.bit[0] === String(bit))
          : null;
        if (!child) break;
        bitsConsumed += child.bit.length;
        if (bitsConsumed > prefixLen) break;
        path.push(nodeId(child));
        node = child;
      }
      return path;
    }

    // For PATRICIA graph mode: highlight node IDs
    if (data.nodes && data.edges) {
      const nodeMap = {};
      data.nodes.forEach((n) => { nodeMap[n.id] = n; });

      // Build full adjacency (forward + back) and forward-only adjacency
      const fullAdj = {};
      const forwardAdj = {};
      data.edges.forEach((e) => {
        if (!fullAdj[e.from]) fullAdj[e.from] = {};
        fullAdj[e.from][e.bit] = { to: e.to, type: e.type };
        if (e.type === "forward") {
          if (!forwardAdj[e.from]) forwardAdj[e.from] = {};
          forwardAdj[e.from][e.bit] = e.to;
        }
      });

      const root = data.nodes.find((n) => n.is_root);
      if (!root) return [];

      // Simulate PATRICIA lookup: follow forward edges, stop at back-pointer
      const path = [root.id];
      let curId = root.id;

      // First step: root always goes to children[0]
      const rootAdj = forwardAdj[curId];
      if (rootAdj && rootAdj["0"] !== undefined) {
        curId = rootAdj["0"];
        path.push(curId);
      } else {
        return path;
      }

      while (true) {
        const node = nodeMap[curId];
        if (!node || node.bit_index < 0) break;
        const bit = (ipNum >>> (31 - node.bit_index)) & 1;
        const edge = fullAdj[curId] && fullAdj[curId][String(bit)];
        if (!edge) break;
        if (edge.type === "forward") {
          curId = edge.to;
          path.push(curId);
        } else {
          // Back-pointer: include target in path (it holds the key)
          if (edge.to !== curId) path.push(edge.to);
          break;
        }
      }
      return path;
    }

    return [];
  }

  function resetZoom() {
    if (!svg) return;
    const container = svg.node().parentElement;
    const { width } = container.getBoundingClientRect();
    svg.transition().duration(duration)
      .call(zoomBehavior.transform, d3.zoomIdentity.translate(width / 2, 40));
  }

  return { setup, render, buildHighlightPath, resetZoom };
})();
