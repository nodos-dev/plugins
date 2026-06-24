#!/usr/bin/env python3
# Generates parity test graphs (*.nosa) for ported nodes.
# Each test: Thread -> TestScheduler drives the graph; the node-under-test's output
# is fed into nos.reflect.IsEqual(expected); IsEqual -> nos.test.Assert with
# EXIT_WITH_STATUS_CODE so `nodos test` gets a pass/fail exit code.
#
# Usage:  python gen_tests.py spec.json
# Writes  Plugins/<plugin>/Tests/<TestName>_Parity.nosa  for each spec entry.
import json, os, sys, uuid, copy

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGINS = os.path.abspath(os.path.join(HERE, "..", ".."))  # Module/plugins

def nid():
    return str(uuid.uuid4())

def pin(name, type_name, show_as, data, extra=None):
    p = {
        "id": nid(), "name": name, "type_name": type_name,
        "show_as": show_as,
        "can_show_as": "OUTPUT_PIN_ONLY" if show_as == "OUTPUT_PIN" else "INPUT_PIN_OR_PROPERTY",
        "visualizers": [], "data": data, "def": data,
        "contents_type": "JobPin", "contents": {}, "orphan_state": {},
    }
    if extra:
        p.update(extra)
    return p

def job(name, class_name, pins, x, y, always=False, plugin_ver=(0, 0, 0)):
    n = {
        "id": nid(), "name": name, "class_name": class_name, "pins": pins,
        "pos": {"x": x, "y": y}, "contents_type": "Job", "contents": {"type": ""},
        "function_category": "Default Node",
        "plugin_version": {"major": plugin_ver[0], "minor": plugin_ver[1], "patch": plugin_ver[2]},
    }
    if always:
        n["always_execute"] = True
    return n

def plugin_id(plugin):
    import glob
    for mf in glob.glob(os.path.join(PLUGINS, "Plugins", plugin, "*.nosplugin")):
        return json.load(open(mf, encoding="utf-8"))["info"]["id"]["name"]
    raise SystemExit("no .nosplugin in " + plugin)

def plugin_name_ver(plugin):
    import glob
    for mf in glob.glob(os.path.join(PLUGINS, "Plugins", plugin, "*.nosplugin")):
        i = json.load(open(mf, encoding="utf-8"))["info"]["id"]
        return {"name": i["name"], "version": i["version"]}
    raise SystemExit("no .nosplugin in " + plugin)

def loaded_modules_for(plugin):
    # The graph needs the test harness (nos.test), IsEqual (nos.reflect), and the
    # plugin under test loaded as modules so their fbs types (e.g.
    # nos.test.AssertionBehaviour) resolve — required for the Assert exit to fire.
    mods = [{"name": "nos.test", "version": "0.3.1"},
            {"name": "nos.reflect", "version": "3.0.0"}]
    tgt = plugin_name_ver(plugin)
    if tgt["name"] not in [m["name"] for m in mods]:
        mods.append(tgt)
    return mods

def fqn(plugin, class_name):
    return class_name if "." in class_name else plugin_id(plugin) + "." + class_name

def load_node_template(plugin, nosnode, class_name):
    path = os.path.join(PLUGINS, "Plugins", plugin, "Nodes", nosnode + ".nosnode")
    doc = json.load(open(path, encoding="utf-8"))
    for entry in doc["nodes"]:
        node = entry.get("node", entry)
        if node.get("class_name") == class_name:
            return copy.deepcopy(node)
    raise SystemExit("class %s not found in %s" % (class_name, path))

def instantiate(plugin, nosnode, class_name, inputs, x, y):
    tmpl = load_node_template(plugin, nosnode, class_name)
    tmpl_pins = tmpl.get("pins", [])
    out_pins = {}
    pins = []
    for tp in tmpl_pins:
        np = {
            "id": nid(), "name": tp["name"], "type_name": tp["type_name"],
            "show_as": tp.get("show_as", "INPUT_PIN"),
            "can_show_as": tp.get("can_show_as", "INPUT_PIN_OR_PROPERTY"),
            "visualizers": [], "contents_type": "JobPin", "contents": {}, "orphan_state": {},
        }
        if tp["name"] in inputs:
            np["data"] = inputs[tp["name"]]
        elif "data" in tp:
            np["data"] = tp["data"]
        if "data" in np:
            np["def"] = np["data"]
        pins.append(np)
        if np["show_as"] == "OUTPUT_PIN":
            out_pins[tp["name"]] = np
    node = job(class_name.split(".")[-1], fqn(plugin, class_name), pins, x, y)
    return node, out_pins

def build_graph(case):
    conns = []
    # Thread (driver)
    th_run = pin("Run", "nos.exe", "OUTPUT_PIN", {})
    th_run["live"] = True
    thread = job("Thread", "nos.Thread",
                 [th_run, pin("Importance", "ulong", "PROPERTY", 0,
                              {"can_show_as": "PROPERTY_ONLY"})],
                 86, -94, always=True)
    # TestScheduler
    ts_run = pin("Run", "nos.exe", "INPUT_PIN", {}, {"can_show_as": "INPUT_PIN_ONLY"})
    ts_sink = pin("Sink", "uint", "INPUT_PIN", 1, {"can_show_as": "INPUT_PIN_ONLY"})
    scheduler = job("TestScheduler", "nos.test.TestScheduler",
                    [ts_run, ts_sink,
                     pin("FreeRun", "bool", "PROPERTY", False),
                     pin("DeltaTime", "nos.fb.vec2u", "PROPERTY", {"x": 1, "y": 60})],
                    324, 7, plugin_ver=(0, 3, 1))
    # node under test
    node, outs = instantiate(case["plugin"], case["nosnode"], case["class"],
                             case.get("inputs", {}), -266, 14)
    out_pin = outs[case["output"]]
    out_type = out_pin["type_name"]
    # IsEqual (concrete output type, not Generic, to avoid type-resolution issues)
    eq_a = pin("A", out_type, "INPUT_PIN", case["expected"])
    eq_b = pin("B", out_type, "INPUT_PIN", case["expected"])
    eq_out = pin("IsEqual", "bool", "OUTPUT_PIN", True)
    iseq = job("IsEqual", "nos.reflect.IsEqual", [eq_a, eq_b, eq_out], -105, 4, plugin_ver=(3, 0, 0))
    # Assert
    as_beh = pin("Behaviour", "nos.test.AssertionBehaviour", "PROPERTY", "EXIT_WITH_STATUS_CODE")
    as_wait = pin("NumFramesToWait", "uint", "PROPERTY", 0)
    as_fc = pin("FrameCount", "uint", "OUTPUT_PIN", 1)
    as_cond = pin("Condition", "bool", "INPUT_PIN", True)
    as_over = pin("NumFramesToAssertOver", "uint", "PROPERTY", 1, {"min": 1})
    assert_node = job("Assert", "nos.test.Assert",
                      [as_beh, as_wait, as_fc, as_cond, as_over], 93, 11, plugin_ver=(0, 3, 1))
    # connections
    conns.append({"from": th_run["id"], "to": ts_run["id"], "id": nid()})
    conns.append({"from": as_fc["id"], "to": ts_sink["id"], "id": nid()})
    conns.append({"from": eq_out["id"], "to": as_cond["id"], "id": nid()})
    conns.append({"from": out_pin["id"], "to": eq_a["id"], "id": nid()})
    graph = {
        "info": {"version": "1.4.0.b0", "loaded_modules": loaded_modules_for(case["plugin"])},
        "graph": {
            "id": nid(), "pos": {"x": 0, "y": 0}, "contents_type": "Graph",
            "contents": {"nodes": [thread, scheduler, assert_node, iseq, node],
                         "connections": conns},
            "plugin_version": {"major": 0, "minor": 0, "patch": 0},
        },
    }
    return graph

def main():
    spec = json.load(open(sys.argv[1], encoding="utf-8"))
    for case in spec:
        g = build_graph(case)
        outdir = os.path.join(PLUGINS, "Plugins", case["plugin"], "Tests")
        os.makedirs(outdir, exist_ok=True)
        fn = os.path.join(outdir, case["name"] + "_Parity.nosa")
        json.dump(g, open(fn, "w", encoding="utf-8"), indent=2)
        print("wrote", os.path.relpath(fn, PLUGINS))

if __name__ == "__main__":
    main()
