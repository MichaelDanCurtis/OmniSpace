// GalaxyCore: the deterministic OmniSpace star map, engine-independent.
//
// This is a bit-for-bit port of client/src/game/galaxy.ts (and rng.ts). It uses
// only the C++ standard library so it can be unit-tested outside Unreal and
// cross-checked against the TypeScript version. Positions are in light years,
// galactic frame. Star ids are "sx.sy.sz.i" plus the authored home "kelvar".
//
// JavaScript semantics that matter for determinism are reproduced on purpose:
// 32-bit wrapping arithmetic in the hashes/PRNG, and the double rounding in
// hashString() (JS numbers are doubles).
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <algorithm>

// M_PI is POSIX, not ISO C++ (missing under MSVC without _USE_MATH_DEFINES).
// Same literal as <math.h>, so results stay bit-exact.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace omni::galaxy
{
	constexpr double SECTOR_SIZE = 40.0;
	constexpr double GALAXY_RADIUS = 46000.0;
	constexpr double LOCAL_DENSITY = 6.5e-4;
	constexpr const char* HOME_ID = "kelvar";
	constexpr double HOME_POS[3] = {-24180.0, 12.0, 6420.0};

	// ------------------------------------------------------------ hashing / rng
	inline uint32_t imul(uint32_t a, uint32_t b) { return a * b; }

	/** JS `hashString` incl. its double-precision result; returns the raw double. */
	inline double hashStringDouble(const std::string& s, uint32_t seed = 0)
	{
		uint32_t h1 = 0xdeadbeefu ^ seed;
		uint32_t h2 = 0x41c6ce57u ^ seed;
		for (unsigned char ch : s)
		{
			h1 = imul(h1 ^ ch, 2654435761u);
			h2 = imul(h2 ^ ch, 1597334677u);
		}
		h1 = imul(h1 ^ (h1 >> 16), 2246822507u) ^ imul(h2 ^ (h2 >> 13), 3266489909u);
		h2 = imul(h2 ^ (h2 >> 16), 2246822507u) ^ imul(h1 ^ (h1 >> 13), 3266489909u);
		return static_cast<double>(h2) * 4294967296.0 + static_cast<double>(h1);
	}

	/** JS `hashString(s) >>> 0` (ToUint32 of the rounded double). */
	inline uint32_t hashString32(const std::string& s, uint32_t seed = 0)
	{
		double v = hashStringDouble(s, seed);
		return static_cast<uint32_t>(std::fmod(v, 4294967296.0));
	}

	inline uint32_t hashInts(std::initializer_list<int32_t> values)
	{
		uint32_t h = 0x811c9dc5u;
		for (int32_t v : values)
		{
			h = imul(h ^ static_cast<uint32_t>(v), 16777619u);
			h ^= h >> 13;
			h = imul(h, 0x5bd1e995u);
			h ^= h >> 15;
		}
		return h;
	}

	/** mulberry32, identical to rng.ts */
	class Rng
	{
	public:
		explicit Rng(uint32_t seed) : s(seed ? seed : 0x9e3779b9u) {}
		double next()
		{
			s += 0x6d2b79f5u;
			uint32_t t = s;
			t = imul(t ^ (t >> 15), t | 1u);
			t ^= t + imul(t ^ (t >> 7), t | 61u);
			return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
		}
		double range(double a, double b) { return a + (b - a) * next(); }
		int32_t intRange(int32_t a, int32_t b) { return static_cast<int32_t>(std::floor(range(a, b + 1))); }
		bool chance(double p) { return next() < p; }
		template <class T> const T& pick(const std::vector<T>& arr)
		{
			size_t i = static_cast<size_t>(std::floor(next() * arr.size())) % arr.size();
			return arr[i];
		}
		double gauss()
		{
			double u = std::max(1e-9, next());
			double v = next();
			return std::sqrt(-2.0 * std::log(u)) * std::cos(2.0 * M_PI * v);
		}
		int32_t poisson(double lambda)
		{
			if (lambda > 30.0) return std::max(0, static_cast<int32_t>(std::round(lambda + std::sqrt(lambda) * gauss())));
			double L = std::exp(-lambda);
			int32_t k = 0;
			double p = 1.0;
			do { k++; p *= next(); } while (p > L);
			return k - 1;
		}
	private:
		uint32_t s;
	};

	inline double clamp(double v, double a, double b) { return v < a ? a : (v > b ? b : v); }

	// ------------------------------------------------------------ star classes
	enum class StarClass : uint8_t { O, B, A, F, G, K, M, L, WD, NS };
	inline const char* className(StarClass c)
	{
		static const char* names[] = {"O", "B", "A", "F", "G", "K", "M", "L", "WD", "NS"};
		return names[static_cast<int>(c)];
	}
	inline const char* classLabel(StarClass c)
	{
		static const char* labels[] = {"O-type blue supergiant", "B-type blue-white star", "A-type white star", "F-type yellow-white star",
									   "G-type yellow star", "K-type orange dwarf", "M-type red dwarf", "L-type brown dwarf", "White dwarf", "Neutron star"};
		return labels[static_cast<int>(c)];
	}
	inline bool scoopable(StarClass c) { return c != StarClass::L && c != StarClass::WD && c != StarClass::NS; }

	struct ClassInfo { double t0, t1, r0, r1, m0, m1, w; };
	inline const ClassInfo& classInfo(StarClass c)
	{
		static const ClassInfo info[] = {
			{30000, 42000, 6.5, 11, 18, 50, 0.4},
			{11000, 28000, 2.4, 6.5, 3, 16, 2},
			{7500, 10000, 1.5, 2.3, 1.6, 2.5, 4.5},
			{6000, 7400, 1.1, 1.5, 1.05, 1.5, 8},
			{5200, 6000, 0.85, 1.15, 0.85, 1.12, 13},
			{3900, 5200, 0.6, 0.88, 0.6, 0.85, 20},
			{2400, 3800, 0.15, 0.6, 0.1, 0.6, 42},
			{1300, 2300, 0.09, 0.12, 0.05, 0.08, 4},
			{8000, 30000, 0.009, 0.015, 0.5, 1.1, 4.5},
			{500000, 900000, 1.4e-5, 1.8e-5, 1.3, 2.1, 1.2},
		};
		return info[static_cast<int>(c)];
	}
	constexpr double CLASS_TOTAL = 0.4 + 2 + 4.5 + 8 + 13 + 20 + 42 + 4 + 4.5 + 1.2;

	struct Star
	{
		std::string id;
		std::string name;
		StarClass cls = StarClass::M;
		double pos[3] = {0, 0, 0};
		double temperature = 0;
		double radiusSolar = 0;
		double massSolar = 0;
		double luminosity = 0;
		std::string color;
		double ageGyr = 0;
		double settlement = 0;
		std::string region;
	};

	// ------------------------------------------------------------ colour
	inline std::string starColor(double t)
	{
		double k = clamp(t, 1000.0, 40000.0) / 100.0;
		double r, g, b;
		if (k <= 66.0)
		{
			r = 255.0;
			g = 99.47 * std::log(k) - 161.12;
			b = k <= 19.0 ? 0.0 : 138.52 * std::log(k - 10.0) - 305.04;
		}
		else
		{
			r = 329.7 * std::pow(k - 60.0, -0.1332);
			g = 288.12 * std::pow(k - 60.0, -0.0755);
			b = 255.0;
		}
		char buf[8];
		auto h = [](double v) { return static_cast<int>(std::round(clamp(v, 0.0, 255.0))); };
		std::snprintf(buf, sizeof buf, "#%02x%02x%02x", h(r), h(g), h(b));
		return buf;
	}

	// ------------------------------------------------------------ density
	inline double rawDensity(double x, double y, double z)
	{
		double r = std::hypot(x, z);
		if (r > GALAXY_RADIUS * 1.15) return 0.0;
		double theta = std::atan2(z, x);
		double pitch = std::tan((13.0 * M_PI) / 180.0);
		double arm = 0.0;
		if (r > 600.0)
		{
			double base = std::log(r / 600.0) / pitch;
			for (int k = 0; k < 4; k++)
			{
				double d = theta - (base + (k * M_PI) / 2.0);
				d = std::atan2(std::sin(d), std::cos(d));
				arm = std::max(arm, std::exp(-(d * d) / (2.0 * 0.22 * 0.22)));
			}
		}
		double disk = std::exp(-r / 13000.0) * (0.35 + 1.4 * arm);
		double h = 260.0 + r * 0.004;
		double vert = std::exp(-std::fabs(y) / h);
		double rr = r / 3200.0;
		double yy = y / 1800.0;
		double bulge = 7.0 * std::exp(-(rr * rr) - yy * yy);
		double edge = clamp((GALAXY_RADIUS * 1.15 - r) / 6000.0, 0.0, 1.0);
		return (disk * vert + bulge) * edge;
	}
	inline double homeRaw() { static const double v = rawDensity(HOME_POS[0], HOME_POS[1], HOME_POS[2]); return v; }
	inline double density(double x, double y, double z) { return (rawDensity(x, y, z) / homeRaw()) * LOCAL_DENSITY; }

	// ------------------------------------------------------------ names
	inline const std::vector<std::string>& PRE()
	{
		static const std::vector<std::string> v = {"Ka", "Ve", "Ta", "Or", "Sel", "Mi", "Dra", "Ny", "Ae", "Ul", "Zer", "Hal", "Ix", "Qua", "Bel", "Cor", "Eos", "Fen", "Gal", "Ith", "Jor", "Kes", "Lio", "Mor", "Nax", "Ost", "Pyr", "Rho", "Sar", "Thal", "Vas", "Wen", "Yl", "Zan", "Ash", "Bri", "Cal", "Dun", "Esk", "Har", "Ka", "Lum", "Nem", "Per", "Riv", "Sol", "Tor", "Var"};
		return v;
	}
	inline const std::vector<std::string>& MID()
	{
		static const std::vector<std::string> v = {"", "", "", "ra", "li", "na", "ro", "the", "ma", "ve", "si", "do", "ri", "la", "an", "el", "is", "or", "ta", "ny"};
		return v;
	}
	inline const std::vector<std::string>& END()
	{
		static const std::vector<std::string> v = {"var", "ris", "tor", "nis", "dor", "ax", "eon", "ia", "us", "an", "os", "eth", "ion", "ar", "el", "yx", "ane", "oth", "ix", "ura", "ka", "en", "is", "ae", "ul", "era", "ost", "ine"};
		return v;
	}
	inline const std::vector<std::string>& CATALOG()
	{
		static const std::vector<std::string> v = {"HD", "KX", "OSC", "NAV", "GSC", "TX"};
		return v;
	}
	inline const std::vector<std::string>& SUFFIX()
	{
		static const std::vector<std::string> v = {"Prime", "Major", "Minor", "Reach", "Gate", "Deep"};
		return v;
	}
	inline const std::vector<std::string>& REGION_A()
	{
		static const std::vector<std::string> v = {"Tanaran", "Ashkar", "Veyl", "Orison", "Cassim", "Morrow", "Halcyon", "Ixion", "Seraph", "Norn", "Pellucid", "Drakon", "Umber", "Lyric", "Quiet", "Ember"};
		return v;
	}
	inline const std::vector<std::string>& REGION_B()
	{
		static const std::vector<std::string> v = {"Reach", "Expanse", "Drift", "Marches", "Shoals", "Verge", "Deep", "Cradle", "Frontier", "Spur", "Gulf", "Hollows"};
		return v;
	}

	inline std::string starName(Rng& rng, bool remote)
	{
		if (remote && rng.chance(0.28))
		{
			std::string cat = rng.pick(CATALOG());
			int n = rng.intRange(100, 9999);
			char letter = static_cast<char>('A' + rng.intRange(0, 25));
			return cat + " " + std::to_string(n) + "-" + letter;
		}
		// JS evaluates left to right; C++ operand order is unspecified, so sequence the draws.
		std::string n = rng.pick(PRE());
		n += rng.pick(MID());
		n += rng.pick(END());
		if (rng.chance(0.12)) n += " " + rng.pick(SUFFIX());
		return n;
	}

	inline std::string regionName(double x, double y, double z)
	{
		uint32_t h = hashInts({static_cast<int32_t>(std::floor(x / 400.0)), static_cast<int32_t>(std::floor(y / 1000.0)), static_cast<int32_t>(std::floor(z / 400.0)), 77});
		Rng r(h);
		std::string a = r.pick(REGION_A());
		std::string b = r.pick(REGION_B());
		return a + " " + b;
	}

	inline double settlementAt(const double pos[3], uint32_t seed)
	{
		double d = std::sqrt((pos[0] - HOME_POS[0]) * (pos[0] - HOME_POS[0]) + (pos[1] - HOME_POS[1]) * (pos[1] - HOME_POS[1]) + (pos[2] - HOME_POS[2]) * (pos[2] - HOME_POS[2]));
		double base = std::pow(clamp(1.0 - d / 95.0, 0.0, 1.0), 1.2);
		double jitter = (static_cast<double>(seed % 1000u) / 1000.0 - 0.5) * 0.25;
		return clamp(base + jitter * (base > 0.0 ? 1.0 : 0.0), 0.0, 1.0);
	}

	// ------------------------------------------------------------ stars
	inline Star makeStar(const std::string& id, const double pos[3], uint32_t seed)
	{
		Rng rng(seed);
		double roll = rng.next() * CLASS_TOTAL;
		StarClass cls = StarClass::M;
		for (int c = 0; c < 10; c++)
		{
			roll -= classInfo(static_cast<StarClass>(c)).w;
			if (roll <= 0.0) { cls = static_cast<StarClass>(c); break; }
		}
		const ClassInfo& info = classInfo(cls);
		double f = rng.next();
		Star s;
		s.id = id;
		s.cls = cls;
		s.pos[0] = pos[0]; s.pos[1] = pos[1]; s.pos[2] = pos[2];
		s.temperature = std::round(info.t0 + (info.t1 - info.t0) * f);
		s.radiusSolar = info.r0 + (info.r1 - info.r0) * (0.3 * rng.next() + 0.7 * f);
		s.massSolar = info.m0 + (info.m1 - info.m0) * f;
		s.luminosity = s.radiusSolar * s.radiusSolar * std::pow(s.temperature / 5778.0, 4.0);
		if (cls == StarClass::NS) s.luminosity = 0.02;
		s.settlement = settlementAt(pos, seed);
		s.name = starName(rng, s.settlement < 0.05);
		s.color = cls == StarClass::NS ? "#cfe0ff" : starColor(s.temperature);
		s.ageGyr = std::round(rng.range(0.2, 11.0) * 10.0) / 10.0;
		s.region = regionName(pos[0], pos[1], pos[2]);
		return s;
	}

	inline Star homeStar()
	{
		Star s;
		s.id = HOME_ID;
		s.name = "Kelvar";
		s.cls = StarClass::G;
		s.pos[0] = HOME_POS[0]; s.pos[1] = HOME_POS[1]; s.pos[2] = HOME_POS[2];
		s.temperature = 5650;
		s.radiusSolar = 1.04;
		s.massSolar = 1.02;
		s.luminosity = 1.04 * 1.04 * std::pow(5650.0 / 5778.0, 4.0);
		s.color = starColor(5650);
		s.ageGyr = 4.1;
		s.settlement = 1.0;
		s.region = "Tanaran Reach";
		return s;
	}

	struct SectorKey
	{
		int32_t x, y, z;
		bool operator==(const SectorKey& o) const { return x == o.x && y == o.y && z == o.z; }
	};
	struct SectorKeyHash
	{
		size_t operator()(const SectorKey& k) const { return hashInts({k.x, k.y, k.z}); }
	};

	inline SectorKey sectorOf(const double p[3])
	{
		return {static_cast<int32_t>(std::floor(p[0] / SECTOR_SIZE)), static_cast<int32_t>(std::floor(p[1] / SECTOR_SIZE)), static_cast<int32_t>(std::floor(p[2] / SECTOR_SIZE))};
	}

	/** Generates a sector's stars. Deterministic; no caching so it stays pure. */
	inline std::vector<Star> generateSector(int32_t sx, int32_t sy, int32_t sz)
	{
		double cx = (sx + 0.5) * SECTOR_SIZE;
		double cy = (sy + 0.5) * SECTOR_SIZE;
		double cz = (sz + 0.5) * SECTOR_SIZE;
		Rng rng(hashInts({sx, sy, sz, 1337}));
		int32_t count = rng.poisson(density(cx, cy, cz) * SECTOR_SIZE * SECTOR_SIZE * SECTOR_SIZE);
		std::vector<Star> stars;
		double centerDensity = std::max(1e-12, density(cx, cy, cz));
		for (int32_t i = 0; i < count; i++)
		{
			double pos[3] = {(sx + rng.next()) * SECTOR_SIZE, (sy + rng.next()) * SECTOR_SIZE, (sz + rng.next()) * SECTOR_SIZE};
			double accept = density(pos[0], pos[1], pos[2]) / centerDensity;
			if (rng.next() > std::min(1.0, accept * 0.85 + 0.15)) continue;
			double dx = pos[0] - HOME_POS[0], dy = pos[1] - HOME_POS[1], dz = pos[2] - HOME_POS[2];
			if (std::sqrt(dx * dx + dy * dy + dz * dz) < 4.2) continue;
			std::string id = std::to_string(sx) + "." + std::to_string(sy) + "." + std::to_string(sz) + "." + std::to_string(i);
			stars.push_back(makeStar(id, pos, hashInts({sx, sy, sz, i, 99})));
		}
		SectorKey hs = sectorOf(HOME_POS);
		if (hs.x == sx && hs.y == sy && hs.z == sz) stars.push_back(homeStar());
		return stars;
	}

	/** Cached sector access + lookups, mirroring the TS module's behaviour. */
	class Galaxy
	{
	public:
		const std::vector<Star>& sector(int32_t sx, int32_t sy, int32_t sz)
		{
			SectorKey k{sx, sy, sz};
			auto it = cache.find(k);
			if (it != cache.end()) return it->second;
			auto res = cache.emplace(k, generateSector(sx, sy, sz));
			for (const Star& s : res.first->second) byId.emplace(s.id, &s);
			return res.first->second;
		}

		const Star* get(const std::string& id)
		{
			auto it = byId.find(id);
			if (it != byId.end()) return it->second;
			if (id == HOME_ID)
			{
				SectorKey hs = sectorOf(HOME_POS);
				sector(hs.x, hs.y, hs.z);
				it = byId.find(id);
				return it != byId.end() ? it->second : nullptr;
			}
			int32_t sx, sy, sz, i;
			if (std::sscanf(id.c_str(), "%d.%d.%d.%d", &sx, &sy, &sz, &i) == 4)
			{
				sector(sx, sy, sz);
				it = byId.find(id);
				return it != byId.end() ? it->second : nullptr;
			}
			return nullptr;
		}

		// Named `nearby` rather than `near`: <windef.h> defines `near` as a macro.
		std::vector<const Star*> nearby(const double p[3], double radius)
		{
			double lo[3] = {p[0] - radius, p[1] - radius, p[2] - radius};
			double hi[3] = {p[0] + radius, p[1] + radius, p[2] + radius};
			SectorKey s0 = sectorOf(lo), s1 = sectorOf(hi);
			std::vector<const Star*> out;
			for (int32_t x = s0.x; x <= s1.x; x++)
				for (int32_t y = s0.y; y <= s1.y; y++)
					for (int32_t z = s0.z; z <= s1.z; z++)
						for (const Star& s : sector(x, y, z))
						{
							double dx = s.pos[0] - p[0], dy = s.pos[1] - p[1], dz = s.pos[2] - p[2];
							if (std::sqrt(dx * dx + dy * dy + dz * dz) <= radius) out.push_back(&s);
						}
			return out;
		}

		static double dist(const Star& a, const Star& b)
		{
			double dx = a.pos[0] - b.pos[0], dy = a.pos[1] - b.pos[1], dz = a.pos[2] - b.pos[2];
			return std::sqrt(dx * dx + dy * dy + dz * dz);
		}

		/** A* over stars within jumpRange; empty when no route. */
		std::vector<const Star*> route(const Star& from, const Star& to, double jumpRange, int maxNodes = 4000)
		{
			if (from.id == to.id) return {&from};
			struct Node { const Star* s; double f, g; };
			std::vector<Node> open{{&from, dist(from, to), 0.0}};
			std::unordered_map<std::string, const Star*> came;
			std::unordered_map<std::string, double> gScore{{from.id, 0.0}};
			std::unordered_map<std::string, bool> closed;
			int expanded = 0;
			while (!open.empty())
			{
				auto best = std::min_element(open.begin(), open.end(), [](const Node& a, const Node& b) { return a.f < b.f; });
				Node cur = *best;
				open.erase(best);
				if (cur.s->id == to.id)
				{
					std::vector<const Star*> path{cur.s};
					std::string k = cur.s->id;
					while (came.count(k)) { const Star* p = came[k]; path.insert(path.begin(), p); k = p->id; }
					return path;
				}
				if (closed[cur.s->id]) continue;
				closed[cur.s->id] = true;
				if (++expanded > maxNodes) return {};
				for (const Star* n : nearby(cur.s->pos, jumpRange))
				{
					if (closed[n->id] || n->id == cur.s->id) continue;
					double g = cur.g + dist(*cur.s, *n) + jumpRange * 0.35;
					auto it = gScore.find(n->id);
					if (it == gScore.end() || g < it->second)
					{
						gScore[n->id] = g;
						came[n->id] = cur.s;
						open.push_back({n, g + dist(*n, to) * 1.05, g});
					}
				}
			}
			return {};
		}

	private:
		std::unordered_map<SectorKey, std::vector<Star>, SectorKeyHash> cache;
		std::unordered_map<std::string, const Star*> byId;
	};

	// Astrophysics helpers (AU)
	inline double frostLineAU(double lum) { return 2.7 * std::sqrt(lum); }
	inline double equilibriumTempK(double lum, double au) { return 278.0 * std::pow(lum, 0.25) / std::sqrt(au); }
	inline uint32_t starSeed(const std::string& id) { return hashString32(id); }
}
