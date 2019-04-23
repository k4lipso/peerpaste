from klein import run, route
import json


def write_json_file():
    with open("data_file.json", "w") as write_file:
        global content
        json.dump(content, write_file)

@route('/', methods=['POST'])
def do_post(request):
    global content
    new_data = json.loads(request.content.read())

    for p in new_data:
        p_short = p[0:5]
        node = { "name" : p_short }
        # content["nodes"] = node
        if node not in content["nodes"]:
            content["nodes"].append(node)
        # if p not in content["nodes"];
        #     content["nodes"].append(node)
        for q in new_data[p]:
            target = { "name" : new_data[p][q][0:5] }
            if target["name"]:
                link = { "source": content["nodes"].index(node), "target" : content["nodes"].index(target)}
                content["links"] = [some_dict for some_dict in content["links"] if some_dict['source'] != content["nodes"].index(node)]
                if link not in content["links"]:
                    if new_data[p][q] is not "":
                        content["links"].append(link)
    # content.update(new_data)
    # write_json_file()
    # print(content)
    # response = json.dumps(dict(the_data=content), indent=4)
    # return response
@route('/')
def home(request):
    global content
    response = json.dumps(content, indent=4)
    # request.setHeader('Content-Type', 'application/json')
    request.setHeader("Access-Control-Allow-Origin", "*")
    return response

content = { "nodes" : [], "links" : [] }
run("localhost", 8080)
