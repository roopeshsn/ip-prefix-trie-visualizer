const App = (() => {
  const prefixes = [];
  let lastLookupResult = null;
  let viewMode = "standard";

  const els = {};

  function cacheDom() {
    els.cidrInput = document.getElementById("cidr-input");
    els.ipInput = document.getElementById("ip-input");
    els.insertBtn = document.getElementById("btn-insert");
    els.deleteBtn = document.getElementById("btn-delete");
    els.lookupBtn = document.getElementById("btn-lookup");
    els.clearBtn = document.getElementById("btn-clear");
    els.resetZoomBtn = document.getElementById("btn-reset-zoom");
    els.viewToggle = document.getElementById("view-toggle");
    els.prefixList = document.getElementById("prefix-list");
    els.lookupResult = document.getElementById("lookup-result");
    els.status = document.getElementById("status");
    els.emptyState = document.getElementById("empty-state");
    els.statNodes = document.getElementById("stat-nodes");
    els.statNodeSize = document.getElementById("stat-node-size");
    els.statTotalMem = document.getElementById("stat-total-mem");
    els.bulkInput = document.getElementById("bulk-input");
    els.bulkImportBtn = document.getElementById("btn-bulk-import");
    els.bulkFile = document.getElementById("bulk-file");
  }

  function bindEvents() {
    els.insertBtn.addEventListener("click", handleInsert);
    els.deleteBtn.addEventListener("click", handleDelete);
    els.lookupBtn.addEventListener("click", handleLookup);
    els.clearBtn.addEventListener("click", handleClear);
    els.resetZoomBtn.addEventListener("click", () => Visualizer.resetZoom());

    els.viewToggle.querySelectorAll(".toggle-btn").forEach((btn) => {
      btn.addEventListener("click", () => {
        viewMode = btn.dataset.mode;
        els.viewToggle.querySelectorAll(".toggle-btn").forEach((b) => b.classList.remove("active"));
        btn.classList.add("active");
        localStorage.setItem("viewMode", viewMode);
        refresh();
      });
    });

    els.bulkImportBtn.addEventListener("click", handleBulkImport);
    els.bulkFile.addEventListener("change", handleFileImport);

    els.cidrInput.addEventListener("keydown", (e) => {
      if (e.key === "Enter") handleInsert();
    });
    els.ipInput.addEventListener("keydown", (e) => {
      if (e.key === "Enter") handleLookup();
    });
  }


  function loadViewMode() {
    const saved = localStorage.getItem("viewMode");
    if (saved && ["standard", "compressed", "patricia"].includes(saved)) {
      viewMode = saved;
      els.viewToggle.querySelectorAll(".toggle-btn").forEach((b) => {
        b.classList.toggle("active", b.dataset.mode === viewMode);
      });
    }
  }

  function handleInsert() {
    const cidr = els.cidrInput.value.trim();
    if (!cidr) return;

    if (!isValidCidr(cidr)) {
      showStatus("Invalid CIDR format. Use: A.B.C.D/N", "error");
      return;
    }

    const result = TrieWasm.insert(cidr);
    if (result !== 0) {
      showStatus("Failed to insert prefix.", "error");
      return;
    }

    const normalized = normalizeCidr(cidr);
    if (!prefixes.includes(normalized)) {
      prefixes.push(normalized);
    }

    els.cidrInput.value = "";
    clearLookup();
    savePrefixes();
    console.log(`[INSERT] ${normalized} — trie now has ${prefixes.length} prefix(es)`);
    showStatus(`Inserted ${normalized}`, "success");
    refresh();
  }

  function parseBulkInput(text) {
    const matches = text.match(/\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\/\d{1,2}/g);
    return matches || [];
  }

  function handleBulkImport() {
    const text = els.bulkInput.value.trim();
    if (!text) return;

    const cidrs = parseBulkInput(text);
    if (cidrs.length === 0) {
      showStatus("No valid CIDRs found in input.", "error");
      return;
    }

    let inserted = 0;
    for (const cidr of cidrs) {
      const result = TrieWasm.insert(cidr);
      if (result === 0) {
        const normalized = normalizeCidr(cidr);
        if (!prefixes.includes(normalized)) {
          prefixes.push(normalized);
          inserted++;
        }
      }
    }

    els.bulkInput.value = "";
    clearLookup();
    savePrefixes();
    console.log(`[BULK IMPORT] ${inserted} prefix(es) imported from ${cidrs.length} parsed`);
    showStatus(`Imported ${inserted} prefix(es)`, "success");
    refresh();
  }

  function handleFileImport(e) {
    const file = e.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = () => {
      els.bulkInput.value = reader.result;
      handleBulkImport();
    };
    reader.readAsText(file);
    e.target.value = "";
  }

  function handleDelete() {
    const cidr = els.cidrInput.value.trim();
    if (!cidr) return;

    if (!isValidCidr(cidr)) {
      showStatus("Invalid CIDR format. Use: A.B.C.D/N", "error");
      return;
    }

    const normalized = normalizeCidr(cidr);
    const result = TrieWasm.remove(normalized);
    if (result !== 0) {
      showStatus("Failed to delete prefix.", "error");
      return;
    }

    const idx = prefixes.indexOf(normalized);
    if (idx !== -1) prefixes.splice(idx, 1);

    els.cidrInput.value = "";
    clearLookup();
    savePrefixes();
    console.log(`[DELETE] ${normalized} — trie now has ${prefixes.length} prefix(es)`);
    showStatus(`Deleted ${normalized}`, "success");
    refresh();
  }

  function handleLookup() {
    const ip = els.ipInput.value.trim();
    if (!ip) return;

    if (!isValidIp(ip)) {
      showStatus("Invalid IP format. Use: A.B.C.D", "error");
      return;
    }

    const match = TrieWasm.lookup(ip, viewMode);
    lastLookupResult = { ip, match };

    if (match) {
      els.lookupResult.textContent = `Match: ${match}`;
      els.lookupResult.className = "lookup-result match";
    } else {
      els.lookupResult.textContent = "No matching prefix";
      els.lookupResult.className = "lookup-result no-match";
    }

    hideStatus();
    refresh();
  }

  function handleClear() {
    prefixes.length = 0;
    TrieWasm.init();
    console.log("[CLEAR] All prefixes removed, tries reset");
    clearLookup();
    hideStatus();
    savePrefixes();
    refresh();
  }

  function removePrefix(cidr) {
    TrieWasm.remove(cidr);
    const idx = prefixes.indexOf(cidr);
    if (idx !== -1) prefixes.splice(idx, 1);
    console.log(`[REMOVE] ${cidr} — trie now has ${prefixes.length} prefix(es)`);
    clearLookup();
    savePrefixes();
    refresh();
  }

  function buildPatriciaHighlight(graphData, ip) {
    const parts = ip.split(".");
    if (parts.length !== 4) return [];
    let ipNum = 0;
    for (let i = 0; i < 4; i++) {
      const v = parseInt(parts[i], 10);
      if (isNaN(v) || v < 0 || v > 255) return [];
      ipNum = (ipNum * 256 + v) >>> 0;
    }

    const nodeMap = {};
    graphData.nodes.forEach((n) => { nodeMap[n.id] = n; });

    // Build adjacency: for each node, store its children edges by bit
    const adj = {};
    graphData.edges.forEach((e) => {
      if (!adj[e.from]) adj[e.from] = {};
      adj[e.from][e.bit] = { to: e.to, type: e.type };
    });

    const root = graphData.nodes.find((n) => n.is_root);
    if (!root) return [];

    const path = [root.id];
    const visited = new Set([root.id]);

    // First step: sentinel root always follows children[0]
    const rootEdge = adj[root.id] && adj[root.id]["0"];
    if (!rootEdge || rootEdge.type !== "forward") return path;

    let curId = rootEdge.to;
    path.push(curId);
    visited.add(curId);

    for (let safety = 0; safety < 64; safety++) {
      const node = nodeMap[curId];
      if (!node || node.bit_index < 0) break;

      const bit = (ipNum >>> (31 - node.bit_index)) & 1;
      const edge = adj[curId] && adj[curId][String(bit)];
      if (!edge) break;

      if (edge.type === "forward" && !visited.has(edge.to)) {
        curId = edge.to;
        path.push(curId);
        visited.add(curId);
      } else {
        // Back-pointer terminates the search
        // Only add target if it's a different node (not self-loop)
        if (edge.to !== curId && !visited.has(edge.to)) {
          path.push(edge.to);
        }
        break;
      }
    }

    return path;
  }

  function annotatePaths(node, path) {
    node._path = path;
    if (node.children) {
      node.children.forEach((child) => annotatePaths(child, path + "/" + child.bit));
    }
  }

  function refresh() {
    renderPrefixList();

    let data, graphData = null;

    if (viewMode === "patricia") {
      graphData = TrieWasm.serializePatricia();
      data = graphData;
    } else if (viewMode === "compressed") {
      data = TrieWasm.serializeCompressed();
    } else {
      data = TrieWasm.serialize();
    }

    if (data && !graphData) annotatePaths(data, "root");

    const hasContent = graphData
      ? (graphData.nodes && graphData.nodes.length > 1)
      : (data && data.children && data.children.length > 0);

    els.emptyState.style.display = hasContent ? "none" : "block";

    let highlightPath = null;
    if (lastLookupResult && lastLookupResult.match && hasContent) {
      if (viewMode === "patricia" && data && data.nodes && data.edges) {
        highlightPath = buildPatriciaHighlight(data, lastLookupResult.ip);
      } else {
        highlightPath = Visualizer.buildHighlightPath(
          data,
          lastLookupResult.ip,
          lastLookupResult.match
        );
      }
    }

    Visualizer.render(data, highlightPath, { isPatricia: viewMode === "patricia" });
    renderStats();

  }

  function renderPrefixList() {
    if (prefixes.length === 0) {
      els.prefixList.innerHTML =
        '<li class="prefix-list-empty">No prefixes inserted</li>';
      return;
    }

    els.prefixList.innerHTML = prefixes
      .map(
        (p) =>
          `<li class="prefix-item">
            <span>${p}</span>
            <button class="remove-btn" data-cidr="${p}">&times;</button>
          </li>`
      )
      .join("");

    els.prefixList.querySelectorAll(".remove-btn").forEach((btn) => {
      btn.addEventListener("click", () => removePrefix(btn.dataset.cidr));
    });
  }

  function renderStats() {
    const s = TrieWasm.stats(viewMode);
    els.statNodes.textContent = s.nodeCount;
    els.statNodeSize.textContent = formatBytes(s.nodeSize);
    els.statTotalMem.textContent = formatBytes(s.totalMemory);
  }

  function formatBytes(bytes) {
    if (bytes < 1024) return bytes + " B";
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB";
    return (bytes / (1024 * 1024)).toFixed(2) + " MB";
  }

  function savePrefixes() {
    localStorage.setItem("prefixes", JSON.stringify(prefixes));
  }

  function loadPrefixes() {
    const saved = localStorage.getItem("prefixes");
    if (!saved) return;
    const list = JSON.parse(saved);
    for (const cidr of list) {
      if (TrieWasm.insert(cidr) === 0 && !prefixes.includes(cidr)) {
        prefixes.push(cidr);
      }
    }
  }

  function clearLookup() {
    lastLookupResult = null;
    els.lookupResult.className = "lookup-result";
    els.lookupResult.textContent = "";
  }

  function showStatus(msg, type) {
    els.status.textContent = msg;
    els.status.className = `status ${type}`;
    setTimeout(() => hideStatus(), 3000);
  }

  function hideStatus() {
    els.status.className = "status";
  }

  function isValidCidr(s) {
    return /^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\/\d{1,2}$/.test(s);
  }

  function isValidIp(s) {
    return /^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$/.test(s);
  }

  function normalizeCidr(cidr) {
    const [ipStr, lenStr] = cidr.split("/");
    const parts = ipStr.split(".").map(Number);
    const len = parseInt(lenStr, 10);
    let ip = ((parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3]) >>> 0;
    if (len === 0) ip = 0;
    else ip = (ip & (0xffffffff << (32 - len))) >>> 0;
    return [
      (ip >>> 24) & 0xff,
      (ip >>> 16) & 0xff,
      (ip >>> 8) & 0xff,
      ip & 0xff,
    ].join(".") + "/" + len;
  }

  async function init() {
    cacheDom();
    loadViewMode();
    await TrieWasm.load("build/trie.wasm");
    loadPrefixes();
    Visualizer.setup(".canvas");
    bindEvents();
    refresh();
  }

  return { init };
})();

document.addEventListener("DOMContentLoaded", () => App.init());
