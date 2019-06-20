import gto


class MyReader(gto.Reader):
    def __init__(self):
        super(MyReader, self).__init__(gto.Reader.RandomAccess)

    def object(self, name, protocol, protocolVersion, info):
        print("Query object \"%s\"" % name)
        return True

    def component(self, name, interpretation, info):
        print("Query component \"%s\"" % name)
        return True

    def property(self, name, interpretation, info):
        print("Query property \"%s\"" % name)
        return True

    def dataRead(self, name, data, info):
        print("Data \"%s.%s.%s\": %d element(s)" % (info.component.object.name, info.component.name, info.name, len(data)))


print("=== Open file")
rdr = MyReader()
rdr.open("/ve/home/GaetanG/Desktop/setDress/output/takeuchi/_nodes_.0500.gto")

print("=== Query and access single component")
ci = rdr.getComponent("particles", "points")
if ci:
    rdr.accessComponent(ci)

print("=== Query and access single property")
pi = rdr.getProperty("particles", "points", "position")
if pi:
    rdr.accessProperty(pi)

rdr.close()


print("=== Print informations")
for o in rdr.objects():
    print("Object \"%s\"" % o.name)

for c in rdr.components():
    print("Component \"%s.%s\"" % (c.object.name, c.name))

for p in rdr.properties():
    print("Property \"%s.%s.%s\"" % (p.component.object.name, p.component.name, p.name))

"""
oi = rdr.findObject("particles")
if oi != -1:
    print("found particles")
    ci = rdr.findComponent(rdr.objectAt(oi), "points")
    if ci != -1:
        print("found particles.points")
        pi = rdr.findProperty(rdr.componentAt(ci), "position")
        if pi != -1:
            print("found particles.points.position")
            print("=> %s" % rdr.accessProperty(rdr.propertyAt(pi)))
        else:
            print("No particles.points.position")
    else:
        print("No particles.points")

rdr.accessComponent(rdr.componentAt(rdr.findComponent("particles", "points")))
"""

print(rdr.findObjects("^p"))
print(rdr.findComponents("particles", "^p"))
print(rdr.findProperties("particles", "points", "[ne]P"))
print(rdr.findProperties("particles", "points", "[pP]osition|PP$"))
