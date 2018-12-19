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
        print(p)
        node = {"name" : p}
        content["nodes"][p] = node
        for q in new_data[p]:
            link = { "source": p, "target" : new_data[p][q]}
            content["links"] = [some_dict for some_dict in content["links"] if some_dict['source'] != p]
            if link not in content["links"]:
                if new_data[p][q] is not "":
                    content["links"].append(link)
            print(q)
            print(new_data[p][q])
    # content.update(new_data)
    write_json_file()
    # response = json.dumps(dict(the_data=content), indent=4)
    # return response
@route('/')
def home(request):
    global content
    response = json.dumps(content, indent=4)
    # request.setHeader('Content-Type', 'application/json')
    request.setHeader("Access-Control-Allow-Origin", "*")
    return response

content = { "nodes" : {}, "links" : [] }
run("localhost", 8080)
