// Dumps stars from the sectors around a point so the C++ port can be compared
// with the TypeScript galaxy. Build: g++ -std=c++17 -O2 -I../Source/OmniSpace/Public galaxy_dump.cpp -o galaxy_dump
#include "Galaxy/GalaxyCore.h"
#include <cstdio>
using namespace omni::galaxy;
int main(int argc, char** argv)
{
	int R = argc > 1 ? atoi(argv[1]) : 1;
	Galaxy g;
	SectorKey h = sectorOf(HOME_POS);
	bool first = true;
	printf("[");
	for (int x = h.x - R; x <= h.x + R; x++)
		for (int y = h.y - R; y <= h.y + R; y++)
			for (int z = h.z - R; z <= h.z + R; z++)
				for (const Star& s : g.sector(x, y, z))
				{
					printf("%s{\"id\":\"%s\",\"name\":\"%s\",\"cls\":\"%s\",\"pos\":[%.6f,%.6f,%.6f],\"t\":%.0f,\"r\":%.6f,\"lum\":%.6g,\"color\":\"%s\",\"age\":%.1f,\"set\":%.4f,\"region\":\"%s\"}",
						   first ? "" : ",", s.id.c_str(), s.name.c_str(), className(s.cls), s.pos[0], s.pos[1], s.pos[2], s.temperature, s.radiusSolar, s.luminosity, s.color.c_str(), s.ageGyr, s.settlement, s.region.c_str());
					first = false;
				}
	printf("]\n");
	if (argc > 2)
	{
		const Star* home = g.get(HOME_ID);
		const Star* dest = g.get(argv[2]);
		if (home && dest)
		{
			auto r = g.route(*home, *dest, 18.0);
			fprintf(stderr, "route:");
			for (auto* s : r) fprintf(stderr, " %s", s->id.c_str());
			fprintf(stderr, "\n");
		}
	}
}
