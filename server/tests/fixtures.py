from omnispace_server.schema import StarDescriptor

def star(settlement=0.8, cls="G", lum=1.0, radius=1.0, sid="-605.0.160.7", name="Harka"):
    return StarDescriptor(id=sid, name=name, cls=cls, pos=[-24180, 12, 6420], temperature=5700, radiusSolar=radius,
                          massSolar=1.0, luminosity=lum, color="#fff4ea", ageGyr=4.0, settlement=settlement, region="Halcyon Spur")
