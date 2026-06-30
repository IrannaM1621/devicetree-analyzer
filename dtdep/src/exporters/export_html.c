#include "exporters/export_html.h"

#include <string.h>
#include <stdlib.h>

static void json_escape(FILE *out, const char *s)
{
    for (const char *c = s; *c; c++) {
        switch (*c) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\n': fputs("\\n",  out); break;
        default:   fputc(*c, out);     break;
        }
    }
}

static const char *category_of(const DtNode *node)
{
    for (const DtProp *p = node->props; p; p = p->next) {
        if (strcmp(p->name, "#clock-cells") == 0)         return "clock";
        if (strcmp(p->name, "interrupt-controller") == 0)  return "interrupt";
        if (strcmp(p->name, "#dma-cells") == 0)            return "dma";
        if (p->kind == PROP_STRING &&
            strcmp(p->name, "compatible") == 0 &&
            strstr(p->str, "simple-bus") != NULL)           return "bus";
    }
    return "peripheral";
}

static const char *edge_label(DepType type)
{
    switch (type) {
    case DEP_CLOCK:     return "clock";
    case DEP_INTERRUPT: return "interrupt";
    case DEP_DMA:       return "dma";
    case DEP_BUS:       return "bus";
    case DEP_POWER:     return "power";
    case DEP_GPIO:      return "gpio";
    case DEP_RESET:     return "reset";
    case DEP_PHY:       return "phy";
    default:            return "dep";
    }
}

static void emit_props_json(FILE *out, const DtNode *node)
{
    int first = 1;
    for (const DtProp *p = node->props; p; p = p->next) {
        if (!first) fputc(',', out);
        first = 0;

        fprintf(out, "{\"name\":\"");
        json_escape(out, p->name);
        fprintf(out, "\",\"value\":\"");

        switch (p->kind) {
        case PROP_EMPTY:
            break;
        case PROP_STRING:
            json_escape(out, p->str);
            break;
        case PROP_CELLS: {
            char buf[512] = {0};
            int pos = 0;
            for (int i = 0; i < p->ncells; i++) {
                pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                                 "%s0x%x", i ? " " : "", p->cells[i]);
            }
            json_escape(out, buf);
            break;
        }
        case PROP_BYTES: {
            char buf[512] = {0};
            int pos = 0;
            for (int i = 0; i < p->nbytes; i++) {
                pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                                 "%s%02x", i ? " " : "", p->bytes[i]);
            }
            json_escape(out, buf);
            break;
        }
        }
        fprintf(out, "\"}");
    }
}

void html_export(const DepGraph *g, const DtResolver *res, FILE *out)
{
    (void)res;

    fprintf(out, "<!doctype html>\n<html><head><meta charset=\"utf-8\">\n");
    fprintf(out, "<title>dtdep — dependency graph</title>\n");
    fprintf(out, "<style>\n");
    fprintf(out,
        "  body{margin:0;font-family:-apple-system,Helvetica,Arial,sans-serif;"
        "background:#fafafa;color:#222;display:flex;height:100vh;overflow:hidden}\n"
        "  #graph{flex:1;position:relative}\n"
        "  svg{width:100%%;height:100%%}\n"
        "  .node{cursor:pointer}\n"
        "  .node rect{stroke-width:1.3;rx:8;ry:8}\n"
        "  .node text{font-size:12px;pointer-events:none;dominant-baseline:central;text-anchor:middle}\n"
        "  .edge{fill:none;stroke-width:1.4}\n"
        "  .edge-label{font-size:9px;fill:#666}\n"
        "  #panel{width:320px;border-left:1px solid #ddd;background:#fff;"
        "padding:16px;overflow-y:auto;box-sizing:border-box}\n"
        "  #panel h2{font-size:15px;margin:0 0 4px}\n"
        "  #panel .cat{display:inline-block;font-size:11px;color:#666;"
        "background:#f0f0f0;padding:2px 8px;border-radius:10px;margin-bottom:12px}\n"
        "  #panel table{width:100%%;border-collapse:collapse;font-size:12px}\n"
        "  #panel td{padding:4px 0;border-bottom:1px solid #eee;vertical-align:top;"
        "word-break:break-all;font-family:monospace}\n"
        "  #panel td.k{color:#888;width:40%%;font-family:-apple-system,sans-serif}\n"
        "  #empty{color:#999;font-size:13px;margin-top:40px;text-align:center}\n"
        "  #legend{position:absolute;top:12px;left:12px;background:#fff;"
        "border:1px solid #ddd;border-radius:8px;padding:10px 14px;font-size:11px}\n"
        "  #legend div{display:flex;align-items:center;gap:6px;margin:3px 0}\n"
        "  #legend span.sw{width:10px;height:10px;border-radius:3px;display:inline-block}\n"
        "</style></head><body>\n");

    fprintf(out, "<div id=\"graph\"><svg id=\"svg\"></svg>"
                  "<div id=\"legend\"></div></div>\n");
    fprintf(out, "<div id=\"panel\"><div id=\"empty\">Click a node to inspect "
                  "its properties and dependencies.</div></div>\n");

    fprintf(out, "<script>\nconst NODES = [\n");
    for (int i = 0; i < g->nnodes; i++) {
        const GraphNode *gn = &g->nodes[i];
        if (strcmp(gn->dt_node->name, "/") == 0 && gn->nedges == 0)
            continue;
        fprintf(out, "{\"id\":\"");
        json_escape(out, gn->dt_node->name);
        fprintf(out, "\",\"category\":\"%s\",\"props\":[", category_of(gn->dt_node));
        emit_props_json(out, gn->dt_node);
        fprintf(out, "]},\n");
    }
    fprintf(out, "];\nconst EDGES = [\n");
    for (int i = 0; i < g->nnodes; i++) {
        const GraphNode *gn = &g->nodes[i];
        for (int j = 0; j < gn->nedges; j++) {
            fprintf(out, "{\"source\":\"");
            json_escape(out, gn->dt_node->name);
            fprintf(out, "\",\"target\":\"");
            json_escape(out, gn->edges[j]->name);
            fprintf(out, "\",\"type\":\"%s\"},\n", edge_label(gn->edge_types[j]));
        }
    }
    fprintf(out, "];\n");

    fprintf(out, "%s", "\n"
"const COLORS = {\n"
"  clock:      {fill:'#E3F2FD', stroke:'#1565C0'},\n"
"  interrupt:  {fill:'#FDECEA', stroke:'#C62828'},\n"
"  dma:        {fill:'#E8F5E9', stroke:'#2E7D32'},\n"
"  bus:        {fill:'#F2F2F2', stroke:'#757575'},\n"
"  peripheral: {fill:'#FDF6E3', stroke:'#B58900'},\n"
"};\n"
"const EDGE_COLORS = {\n"
"  clock:'#1565C0', interrupt:'#C62828', dma:'#2E7D32', bus:'#9E9E9E',\n"
"  power:'#EF6C00', gpio:'#6A1B9A', reset:'#5D4037', phy:'#00695C', dep:'#424242'\n"
"};\n"
"\n"
"const svg = document.getElementById('svg');\n"
"const W = window.innerWidth - 320, H = window.innerHeight;\n"
"svg.setAttribute('viewBox', `0 0 ${W} ${H}`);\n"
"\n"
"const providers = NODES.filter(n => n.category !== 'peripheral');\n"
"const consumers = NODES.filter(n => n.category === 'peripheral');\n"
"const pos = {};\n"
"function layout(list, x) {\n"
"  const gap = H / (list.length + 1);\n"
"  list.forEach((n, i) => { pos[n.id] = {x, y: gap * (i + 1)}; });\n"
"}\n"
"layout(consumers, W * 0.25);\n"
"layout(providers, W * 0.75);\n"
"\n"
"const ns = 'http://www.w3.org/2000/svg';\n"
"function el(tag, attrs) {\n"
"  const e = document.createElementNS(ns, tag);\n"
"  for (const k in attrs) e.setAttribute(k, attrs[k]);\n"
"  return e;\n"
"}\n"
"\n"
"EDGES.forEach(e => {\n"
"  const a = pos[e.source], b = pos[e.target];\n"
"  if (!a || !b) return;\n"
"  const mx = (a.x + b.x) / 2;\n"
"  const path = el('path', {\n"
"    d: `M ${a.x+50} ${a.y} C ${mx} ${a.y}, ${mx} ${b.y}, ${b.x-50} ${b.y}`,\n"
"    class: 'edge', stroke: EDGE_COLORS[e.type] || '#999',\n"
"    'stroke-dasharray': e.type === 'bus' ? '4,3' : 'none',\n"
"    'marker-end': 'url(#arrow)'\n"
"  });\n"
"  svg.appendChild(path);\n"
"  const lbl = el('text', {\n"
"    x: mx, y: (a.y + b.y) / 2 - 4, class: 'edge-label', 'text-anchor':'middle'\n"
"  });\n"
"  lbl.textContent = e.type;\n"
"  svg.appendChild(lbl);\n"
"});\n"
"\n"
"const defs = el('defs', {});\n"
"defs.innerHTML = '<marker id=\"arrow\" viewBox=\"0 0 10 10\" refX=\"8\" refY=\"5\" "
"markerWidth=\"6\" markerHeight=\"6\" orient=\"auto-start-reverse\">"
"<path d=\"M0 0 L10 5 L0 10 z\" fill=\"#999\"/></marker>';\n"
"svg.prepend(defs);\n"
"\n"
"NODES.forEach(n => {\n"
"  const p = pos[n.id];\n"
"  if (!p) return;\n"
"  const c = COLORS[n.category] || COLORS.peripheral;\n"
"  const g = el('g', {class:'node', transform:`translate(${p.x-50},${p.y-16})`});\n"
"  g.appendChild(el('rect', {width:100, height:32, fill:c.fill, stroke:c.stroke}));\n"
"  const t = el('text', {x:50, y:16});\n"
"  t.textContent = n.id.length > 14 ? n.id.slice(0,13)+'\\u2026' : n.id;\n"
"  g.appendChild(t);\n"
"  g.addEventListener('click', () => showPanel(n));\n"
"  svg.appendChild(g);\n"
"});\n"
"\n"
"const legend = document.getElementById('legend');\n"
"Object.keys(COLORS).forEach(cat => {\n"
"  const row = document.createElement('div');\n"
"  row.innerHTML = `<span class=\"sw\" style=\"background:${COLORS[cat].fill};"
"border:1px solid ${COLORS[cat].stroke}\"></span>${cat}`;\n"
"  legend.appendChild(row);\n"
"});\n"
"\n"
"function showPanel(n) {\n"
"  const panel = document.getElementById('panel');\n"
"  const deps = EDGES.filter(e => e.source === n.id);\n"
"  const revDeps = EDGES.filter(e => e.target === n.id);\n"
"  let html = `<h2>${n.id}</h2><span class=\"cat\">${n.category}</span>`;\n"
"  html += '<table>';\n"
"  n.props.forEach(p => {\n"
"    html += `<tr><td class=\"k\">${p.name}</td><td>${p.value || '(empty)'}</td></tr>`;\n"
"  });\n"
"  html += '</table>';\n"
"  if (deps.length) {\n"
"    html += '<h2 style=\"margin-top:16px\">Depends on</h2><table>';\n"
"    deps.forEach(d => { html += `<tr><td class=\"k\">${d.type}</td><td>${d.target}</td></tr>`; });\n"
"    html += '</table>';\n"
"  }\n"
"  if (revDeps.length) {\n"
"    html += '<h2 style=\"margin-top:16px\">Used by</h2><table>';\n"
"    revDeps.forEach(d => { html += `<tr><td class=\"k\">${d.type}</td><td>${d.source}</td></tr>`; });\n"
"    html += '</table>';\n"
"  }\n"
"  panel.innerHTML = html;\n"
"}\n"
"</script>\n</body></html>\n");
}
