// world.cpp: core map management stuff
#include "engine.h"
#include "../shared/ents.h"
#include "../game/entities/player.h"
#include "../game/entities/playerstart.h"
#include <cassert>

VARR(mapversion, 1, MAPVERSION, 0);
VARNR(mapscale, worldscale, 1, 0, 0);
VARNR(mapsize, worldsize, 1, 0, 0);
SVARR(maptitle, "Untitled Map by Unknown");
VARNR(emptymap, _emptymap, 1, 0, 0);

VAR(octaentsize, 0, 64, 1024);
VAR(entselradius, 0, 2, 10);

static inline void transformbb(const entities::classes::CoreEntity *e, vec &center, vec &radius)
{
	if(e->attr5 > 0) { float scale = e->attr5/100.0f; center.mul(scale); radius.mul(scale); }
	rotatebb(center, radius, e->attr2, e->attr3, e->attr4);
}

static inline void mmboundbox(const entities::classes::CoreEntity *e, model *m, vec &center, vec &radius)
{
    m->boundbox(center, radius);
    transformbb(e, center, radius);
}

static inline void mmcollisionbox(const entities::classes::CoreEntity *e, model *m, vec &center, vec &radius)
{
    m->collisionbox(center, radius);
    transformbb(e, center, radius);
}

static inline void decalboundbox(const entities::classes::CoreEntity *e, DecalSlot &s, vec &center, vec &radius)
{
	float size = max(float(e->attr5), 1.0f);
    center = vec(0, s.depth * size/2, 0);
    radius = vec(size/2, s.depth * size/2, size/2);
	rotatebb(center, radius, e->attr2, e->attr3, e->attr4);
}

bool getentboundingbox(const entities::classes::CoreEntity *e, ivec &o, ivec &r)
{
	return e->getBoundingBox(entselradius, o, r);
	/*switch(e->et_type)
    {
        case ET_EMPTY:
            return false;
        case ET_DECAL:
            {
				DecalSlot &s = lookupdecalslot(e->attr1, false);
                vec center, radius;
                decalboundbox(e, s, center, radius);
				center.add(e->o);
                radius.max(entselradius);
                o = ivec(vec(center).sub(radius));
                r = ivec(vec(center).add(radius).add(1));
                break;
            }
        case ET_MAPMODEL:
			if(model *m = loadmapmodel(e->model_idx))
            {
                vec center, radius;
                mmboundbox(e, m, center, radius);
				center.add(e->o);
                radius.max(entselradius);
                o = ivec(vec(center).sub(radius));
                r = ivec(vec(center).add(radius).add(1));
                break;
            }
        // invisible mapmodels use entselradius
        default:
			o = ivec(vec(e->o).sub(entselradius));
			r = ivec(vec(e->o).add(entselradius+1));
            break;
    }
    return true;*/
}

enum
{
    MODOE_ADD      = 1<<0,
    MODOE_UPDATEBB = 1<<1,
    MODOE_CHANGED  = 1<<2
};

void modifyoctaentity(int flags, int id, entities::classes::CoreEntity *e, cube *c, const ivec &cor, int size, const ivec &bo, const ivec &br, int leafsize, vtxarray *lastva = NULL)
{
    loopoctabox(cor, size, bo, br)
    {
        ivec o(i, cor, size);
        vtxarray *va = c[i].ext && c[i].ext->va ? c[i].ext->va : lastva;
        if(c[i].children != NULL && size > leafsize)
            modifyoctaentity(flags, id, e, c[i].children, o, size>>1, bo, br, leafsize, va);
        else if(flags&MODOE_ADD)
        {
            if(!c[i].ext || !c[i].ext->ents) ext(c[i]).ents = new octaentities(o, size);
            octaentities &oe = *c[i].ext->ents;
			switch(e->et_type)
            {
                case ET_DECAL:
                    if(va)
                    {
                        va->bbmin.x = -1;
                        if(oe.decals.empty()) va->decals.add(&oe);
                    }
                    oe.decals.add(id);
                    oe.bbmin.min(bo).max(oe.o);
                    oe.bbmax.max(br).min(ivec(oe.o).add(oe.size));
                    break;
                // WatIsDeze: Testing for map models.
//                case ET_MAPMODEL:
//                    if(loadmapmodel(e->attr1))
//                    {
//                        if(va)
//                        {
//                            va->bbmin.x = -1;
//                            if(oe.mapmodels.empty()) va->mapmodels.add(&oe);
//                        }
//                        oe.mapmodels.add(id);
//                        oe.bbmin.min(bo).max(oe.o);
//                        oe.bbmax.max(br).min(ivec(oe.o).add(oe.size));
//                        break;
//                    }
                    case ET_MAPMODEL:
					if(loadmapmodel(e->model_idx))
                        {
                            if(va)
                            {
                                va->bbmin.x = -1;
                                if(oe.mapmodels.empty()) va->mapmodels.add(&oe);
                            }
                            oe.mapmodels.add(id);
                            oe.bbmin.min(bo).max(oe.o);
                            oe.bbmax.max(br).min(ivec(oe.o).add(oe.size));
                            break;
                        }

                    // invisible mapmodel
                default:
                    oe.other.add(id);
                    break;
            }

        }
        else if(c[i].ext && c[i].ext->ents)
        {
            octaentities &oe = *c[i].ext->ents;
			const auto& ents = entities::getents();
			switch(e->et_type)
            {
                case ET_DECAL: {
                    oe.decals.removeobj(id);
                    if(va)
                    {
                        va->bbmin.x = -1;
                        if(oe.decals.empty()) va->decals.removeobj(&oe);
                    }
                    oe.bbmin = oe.bbmax = oe.o;
                    oe.bbmin.add(oe.size);
                    loopvj(oe.decals)
                    {
                        auto e = ents[oe.decals[j]];
                        ivec eo, er;
                        if(getentboundingbox(e, eo, er))
                        {
                            oe.bbmin.min(eo);
                            oe.bbmax.max(er);
                        }
                    }
                    oe.bbmin.max(oe.o);
                    oe.bbmax.min(ivec(oe.o).add(oe.size));
				} break;
//                case ET_MAPMODEL:
//                    if(loadmapmodel(e->attr1))
//                    {
//                        oe.mapmodels.removeobj(id);
//                        if(va)
//                        {
//                            va->bbmin.x = -1;
//                            if(oe.mapmodels.empty()) va->mapmodels.removeobj(&oe);
//                        }
//                        oe.bbmin = oe.bbmax = oe.o;
//                        oe.bbmin.add(oe.size);
//                        loopvj(oe.mapmodels)
//                        {
//                            entities::classes::CoreEntity *e = entities::getents()[oe.mapmodels[j]];
//                            ivec eo, er;
//                            if(getentboundingbox(e, eo, er))
//                            {
//                                oe.bbmin.min(eo);
//                                oe.bbmax.max(er);
//                            }
//                        }
//                        oe.bbmin.max(oe.o);
//                        oe.bbmax.min(ivec(oe.o).add(oe.size));
//                        break;
//                    }
				case ET_MAPMODEL:
					if(loadmapmodel(e->model_idx))
					{
						oe.mapmodels.removeobj(id);
						if(va)
						{
							va->bbmin.x = -1;
							if(oe.mapmodels.empty()) va->mapmodels.removeobj(&oe);
						}
						oe.bbmin = oe.bbmax = oe.o;
						oe.bbmin.add(oe.size);
						loopvj(oe.mapmodels)
						{
							auto e = ents[oe.mapmodels[j]];
							
							ivec eo, er;
							if(getentboundingbox(e, eo, er))
							{
								oe.bbmin.min(eo);
								oe.bbmax.max(er);
							}
						}
						oe.bbmin.max(oe.o);
						oe.bbmax.min(ivec(oe.o).add(oe.size));
						break;
					}
                    // invisible mapmodel
                default:
                    oe.other.removeobj(id);
                    break;
            }
            if(oe.mapmodels.empty() && oe.decals.empty() && oe.other.empty())
                freeoctaentities(c[i]);
        }
        if(c[i].ext && c[i].ext->ents) c[i].ext->ents->query = NULL;
        if(va && va!=lastva)
        {
            if(lastva)
            {
                if(va->bbmin.x < 0) lastva->bbmin.x = -1;
            }
            else if(flags&MODOE_UPDATEBB) updatevabb(va);
        }
    }
}

vector<int> outsideents;
int spotlights = 0, volumetriclights = 0, nospeclights = 0;

static bool modifyoctaent(int flags, int id, entities::classes::CoreEntity *e)
{
	if(flags&MODOE_ADD ? e->flags&entities::EntityFlags::EF_OCTA : !(e->flags&entities::EntityFlags::EF_OCTA)) return false;

    ivec o, r;
    if(!getentboundingbox(e, o, r)) return false;

	if(!insideworld(e->o))
    {
        int idx = outsideents.find(id);
        if(flags&MODOE_ADD)
        {
            if(idx < 0) outsideents.add(id);
        }
        else if(idx >= 0) outsideents.removeunordered(idx);
    }
    else
    {
        int leafsize = octaentsize, limit = max(r.x - o.x, max(r.y - o.y, r.z - o.z));
        while(leafsize < limit) leafsize *= 2;
        int diff = ~(leafsize-1) & ((o.x^r.x)|(o.y^r.y)|(o.z^r.z));
        if(diff && (limit > octaentsize/2 || diff < leafsize*2)) leafsize *= 2;
        modifyoctaentity(flags, id, e, worldroot, ivec(0, 0, 0), worldsize>>1, o, r, leafsize);
    }
	e->flags ^= entities::EntityFlags::EF_OCTA;
	switch(e->et_type)
    {
        case ET_LIGHT:
            clearlightcache(id);
			if(e->attr5&L_VOLUMETRIC) { if(flags&MODOE_ADD) volumetriclights++; else --volumetriclights; }
			if(e->attr5&L_NOSPEC) { if(!(flags&MODOE_ADD ? nospeclights++ : --nospeclights)) cleardeferredlightshaders(); }
            break;
        case ET_SPOTLIGHT: if(!(flags&MODOE_ADD ? spotlights++ : --spotlights)) { cleardeferredlightshaders(); cleanupvolumetric(); } break;
        case ET_PARTICLES: clearparticleemitters(); break;
        case ET_DECAL: if(flags&MODOE_CHANGED) changed(o, r, false); break;
    }
    return true;
}

static inline bool modifyoctaent(int flags, int id)
{
    auto &ents = entities::getents();
	return ents.inrange(id) && modifyoctaent(flags, id, ents[id]);
}

static inline void addentity(int id)        { modifyoctaent(MODOE_ADD|MODOE_UPDATEBB, id); }
static inline void addentityedit(int id)    { modifyoctaent(MODOE_ADD|MODOE_UPDATEBB|MODOE_CHANGED, id); }
static inline void removeentity(int id)     { modifyoctaent(MODOE_UPDATEBB, id); }
static inline void removeentityedit(int id) { modifyoctaent(MODOE_UPDATEBB|MODOE_CHANGED, id); }

void freeoctaentities(cube &c)
{
    if(!c.ext) return;
    if(entities::getents().length())
    {
        while(c.ext->ents && !c.ext->ents->mapmodels.empty()) removeentity(c.ext->ents->mapmodels.pop());
        while(c.ext->ents && !c.ext->ents->decals.empty())    removeentity(c.ext->ents->decals.pop());
        while(c.ext->ents && !c.ext->ents->other.empty())     removeentity(c.ext->ents->other.pop());
    }
    if(c.ext->ents)
    {
        delete c.ext->ents;
        c.ext->ents = NULL;
    }
}

void entitiesinoctanodes()
{
    auto &ents = entities::getents();

    loopv(ents) {
        if (ents.inrange(i)) {
            if (ents[i] != NULL)
            {
				modifyoctaent(MODOE_ADD, i, ents[i]);
                conoutf("entitiesinoctanodes: %d", ents.length());
            }
        }
    }
}

static inline void findents(octaentities &oe, int low, int high, bool notspawned, const vec &pos, const vec &invradius, vector<int> &found)
{
    auto &ents = entities::getents();
    loopv(oe.other)
    {
        int id = oe.other[i];

        if (ents.inrange(id)) {
            auto e = ents[id];
            // TODO: Fix this, et_type? and ent_type?
			if(e->et_type >= low && e->et_type <= high && (e->spawned() || notspawned) && vec(e->o).sub(pos).mul(invradius).squaredlen() <= 1) found.add(id);
        }
    }
}

static inline void findents(cube *c, const ivec &o, int size, const ivec &bo, const ivec &br, int low, int high, bool notspawned, const vec &pos, const vec &invradius, vector<int> &found)
{
    loopoctabox(o, size, bo, br)
    {
        if(c[i].ext && c[i].ext->ents) findents(*c[i].ext->ents, low, high, notspawned, pos, invradius, found);
        if(c[i].children && size > octaentsize)
        {
            ivec co(i, o, size);
            findents(c[i].children, co, size>>1, bo, br, low, high, notspawned, pos, invradius, found);
        }
    }
}

void findents(int low, int high, bool notspawned, const vec &pos, const vec &radius, vector<int> &found)
{
    vec invradius(1/radius.x, 1/radius.y, 1/radius.z);
    ivec bo(vec(pos).sub(radius).sub(1)),
         br(vec(pos).add(radius).add(1));
    int diff = (bo.x^br.x) | (bo.y^br.y) | (bo.z^br.z) | octaentsize,
        scale = worldscale-1;
    if(diff&~((1<<scale)-1) || uint(bo.x|bo.y|bo.z|br.x|br.y|br.z) >= uint(worldsize))
    {
        findents(worldroot, ivec(0, 0, 0), 1<<scale, bo, br, low, high, notspawned, pos, invradius, found);
        return;
    }
    cube *c = &worldroot[octastep(bo.x, bo.y, bo.z, scale)];
    if(c->ext && c->ext->ents) findents(*c->ext->ents, low, high, notspawned, pos, invradius, found);
    scale--;
    while(c->children && !(diff&(1<<scale)))
    {
        c = &c->children[octastep(bo.x, bo.y, bo.z, scale)];
        if(c->ext && c->ext->ents) findents(*c->ext->ents, low, high, notspawned, pos, invradius, found);
        scale--;
    }
    if(c->children && 1<<scale >= octaentsize) findents(c->children, ivec(bo).mask(~((2<<scale)-1)), 1<<scale, bo, br, low, high, notspawned, pos, invradius, found);
}

char *entname(entities::classes::CoreEntity *e)
{
    static std::string fullentname;
	std::string classname = e->classname;
	std::string name = e->name;
    if(!name.empty())
    {
        fullentname = classname + ":" + name;
    }
    return (char*)fullentname.c_str();
}

extern selinfo sel;
extern bool havesel;
int entlooplevel = 0;
int efocus = -1, enthover = -1, entorient = -1, oldhover = -1;
bool undonext = true;

VARF(entediting, 0, 0, 1, { if(!entediting) { entcancel(); efocus = enthover = -1; } });

bool noentedit()
{
    if(!editmode) { conoutf(CON_ERROR, "operation only allowed in edit mode"); return true; }
    return !entediting;
}

bool pointinsel(const selinfo &sel, const vec &o)
{
    return(o.x <= sel.o.x+sel.s.x*sel.grid
        && o.x >= sel.o.x
        && o.y <= sel.o.y+sel.s.y*sel.grid
        && o.y >= sel.o.y
        && o.z <= sel.o.z+sel.s.z*sel.grid
        && o.z >= sel.o.z);
}

vector<int> entgroup;

bool haveselent()
{
    return entgroup.length() > 0;
}

SCRIPTEXPORT void entcancel()
{
    entgroup.shrink(0);
}

void entadd(int id)
{
    undonext = true;
    entgroup.add(id);
}

undoblock *newundoent()
{
    int numents = entgroup.length();
    if(numents <= 0) return NULL;
    undoblock *u = (undoblock *)new uchar[sizeof(undoblock) + numents*sizeof(undoent)];
    u->numents = numents;
    undoent *e = (undoent *)(u + 1);
    auto& ents = entities::getents();
    loopv(entgroup)
    {
        e->i = entgroup[i];
        assert(ents.length() > entgroup[i]);
        e->e = ents[entgroup[i]];
        e++;
    }
    return u;
}

void makeundoent()
{
    if(!undonext) return;
    undonext = false;
    oldhover = enthover;
    undoblock *u = newundoent();
    if(u) addundo(u);
}

void detachentity(entities::classes::CoreEntity *e)
{
	if(!e->attached) return;
	e->attached->attached = NULL;
	e->attached = NULL;
}

VAR(attachradius, 1, 100, 1000);

void attachentity(entities::classes::CoreEntity *e)
{
    switch(e->et_type)
    {
        case ET_SPOTLIGHT:
            break;

        default:
            if(e->et_type<ET_GAMESPECIFIC || !entities::mayattach(e)) return;
            break;
    }

	detachentity(e);

    auto &ents = entities::getents();
    int closest = -1;
    float closedist = 1e10f;
    loopv(ents)
    {
        auto a = ents[i];
        if (!a) continue;
        
        if(a->attached) continue;
        switch(e->et_type)
        {
            case ET_SPOTLIGHT:
                if(a->et_type!=ET_LIGHT) continue;
                break;

            default:
                if(e->et_type<ET_GAMESPECIFIC || !entities::attachent(e, a)) continue;
                break;
        }
        float dist = e->o.dist(a->o);
        if(dist < closedist)
        {
            closest = i;
            closedist = dist;
        }
    }
    if(closedist>attachradius) return;
    e->attached = ents[closest];
    ents[closest]->attached = e;
}

void attachentities()
{
    auto  &ents = entities::getents();
    loopv(ents) attachentity(ents[i]);
}

// convenience macros implicitly define:
// e         entity, currently edited ent
// n         int,    index to currently edited ent
#define addimplicit(f)    { if(entgroup.empty() && enthover>=0) { entadd(enthover); undonext = (enthover != oldhover); f; entgroup.drop(); } else f; }
#define entfocusv(i, f, v){ int n = efocus = (i); if(n>=0) { entities::classes::CoreEntity *e = v[n]; f; } }
#define entfocus(i, f)    entfocusv(i, f, entities::getents())
#define enteditv(i, f, v) \
{ \
    entfocusv(i, \
    { \
        int old_et_type = e->et_type; \
        int old_ent_type = e->ent_type; \
        int old_game_type = e->game_type; \
        removeentityedit(n);  \
        f; \
        if(old_et_type!=e->et_type) detachentity(e); \
        if(e->et_type!=ET_EMPTY) { addentityedit(n); if(old_et_type!=e->et_type) attachentity(e); } \
        entities::editent(n, true); \
        clearshadowcache(); \
    }, v); \
}
#define entedit(i, f)   enteditv(i, f, entities::getents())
#define addgroup(exp)   { auto &ents = entities::getents(); loopv(ents) entfocusv(i, if(exp) entadd(n), ents); }
#define setgroup(exp)   { entcancel(); addgroup(exp); }
#define groupeditloop(f){ auto &ents = entities::getents(); entlooplevel++; int _ = efocus; loopv(entgroup) enteditv(entgroup[i], f, ents); efocus = _; entlooplevel--; }
#define groupeditpure(f){ if(entlooplevel>0) { entedit(efocus, f); } else { groupeditloop(f); commitchanges(); } }
#define groupeditundo(f){ makeundoent(); groupeditpure(f); }
#define groupedit(f)    { addimplicit(groupeditundo(f)); }

vec getselpos()
{
    auto &ents = entities::getents();
    if(entgroup.length() && ents.inrange(entgroup[0])) return ents[entgroup[0]]->o;
    if(ents.inrange(enthover)) return ents[enthover]->o;
    return vec(sel.o);
}

undoblock *copyundoents(undoblock *u)
{
    entcancel();
    undoent *e = u->ents();
    loopi(u->numents)
        entadd(e[i].i);
    undoblock *c = newundoent();
    loopi(u->numents) if(e[i].e->et_type==ET_EMPTY)
        entgroup.removeobj(e[i].i);
    return c;
}

void pasteundoent(int idx,  entities::classes::CoreEntity* ue)
{
    if(idx < 0 || idx >= MAXENTS) return;
    auto &ents = entities::getents();
    while(ents.length() < idx)
    {
		auto ne = entities::newgameentity("");
		ents.add(ne)->et_type = ET_EMPTY;
	}
    int efocus = -1;
    entedit(idx, e = ue);
}

void pasteundoents(undoblock *u)
{
    undoent *ue = u->ents();
    loopi(u->numents) pasteundoent(ue[i].i, ue[i].e);
}

SCRIPTEXPORT void entflip()
{
    if(noentedit()) return;
    int d = dimension(sel.orient);
    float mid = sel.s[d]*sel.grid/2+sel.o[d];
	groupeditundo(e->o[d] -= (e->o[d]-mid)*2);
}

SCRIPTEXPORT void entrotate(int *cw)
{
    if(noentedit()) return;
    int d = dimension(sel.orient);
    int dd = (*cw<0) == dimcoord(sel.orient) ? R[d] : C[d];
    float mid = sel.s[dd]*sel.grid/2+sel.o[dd];
    vec s(sel.o.v);
    groupeditundo(
        e->o[dd] -= (e->o[dd]-mid)*2;
        e->o.sub(s);
        swap(e->o[R[d]], e->o[C[d]]);
        e->o.add(s);
    );
}

void entselectionbox(const entities::classes::CoreEntity *e, vec &eo, vec &es)
{
    model *m = NULL;
    const char *mname = entities::entmodel(e);
    if(mname && (m = loadmodel(mname)))
    {
        m->collisionbox(eo, es);
        if(es.x > es.y) es.y = es.x; else es.x = es.y; // square
        es.z = (es.z + eo.z + 1 + entselradius)/2; // enclose ent radius box and model box
        eo.x += e->o.x;
        eo.y += e->o.y;
        eo.z = e->o.z - entselradius + es.z;
    }
	else if(e->et_type == ET_MAPMODEL && (m = loadmapmodel(e->model_idx)))
    {
        mmcollisionbox(e, m, eo, es);
        es.max(entselradius);
        eo.add(e->o);
    }
    else if(e->et_type == ET_DECAL)
    {
        DecalSlot &s = lookupdecalslot(e->attr1, false);
        decalboundbox(e, s, eo, es);
        es.max(entselradius);
        eo.add(e->o);
    }
    else
    {
        es = vec(entselradius);
        eo = e->o;
    }
    eo.sub(es);
    es.mul(2);
}

VAR(entselsnap, 0, 0, 1);
VAR(entmovingshadow, 0, 1, 1);

extern void boxs(int orient, vec o, const vec &s, float size);
extern void boxs(int orient, vec o, const vec &s);
extern void boxs3D(const vec &o, vec s, int g);
extern bool editmoveplane(const vec &o, const vec &ray, int d, float off, vec &handle, vec &dest, bool first);

int entmoving = 0;

void entdrag(const vec &ray)
{
    if(noentedit() || !haveselent()) return;

    float r = 0, c = 0;
    static vec dest, handle;
    vec eo, es;
    int d = dimension(entorient),
        dc= dimcoord(entorient);

    entfocus(entgroup.last(),
        entselectionbox(e, eo, es);

        if(!editmoveplane(e->o, ray, d, eo[d] + (dc ? es[d] : 0), handle, dest, entmoving==1))
            return;

        ivec g(dest);
        int z = g[d]&(~(sel.grid-1));
        g.add(sel.grid/2).mask(~(sel.grid-1));
        g[d] = z;

        r = (entselsnap ? g[R[d]] : dest[R[d]]) - e->o[R[d]];
        c = (entselsnap ? g[C[d]] : dest[C[d]]) - e->o[C[d]];
    );

    if(entmoving==1) makeundoent();
    groupeditpure(e->o[R[d]] += r; e->o[C[d]] += c);
    entmoving = 2;
}

VAR(showentradius, 0, 1, 1);

void renderentring(const entities::classes::CoreEntity *e, float radius, int axis)
{
    if(radius <= 0) return;
    gle::defvertex();
    gle::begin(GL_LINE_LOOP);
    loopi(15)
    {
        vec p(e->o);
        const vec2 &sc = sincos360[i*(360/15)];
        p[axis>=2 ? 1 : 0] += radius*sc.x;
        p[axis>=1 ? 2 : 1] += radius*sc.y;
        gle::attrib(p);
    }
    xtraverts += gle::end();
}

void renderentsphere(const entities::classes::CoreEntity *e, float radius)
{
    if(radius <= 0) return;
    loopk(3) renderentring(e, radius, k);
}

void renderentattachment(const entities::classes::CoreEntity *e)
{
    if(!e->attached) return;
    gle::defvertex();
    gle::begin(GL_LINES);
    gle::attrib(e->o);
    gle::attrib(e->attached->o);
    xtraverts += gle::end();
}

void renderentarrow(const entities::classes::CoreEntity *e, const vec &dir, float radius)
{
    if(radius <= 0) return;
    float arrowsize = min(radius/8, 0.5f);
    vec target = vec(dir).mul(radius).add(e->o), arrowbase = vec(dir).mul(radius - arrowsize).add(e->o), spoke;
    spoke.orthogonal(dir);
    spoke.normalize();
    spoke.mul(arrowsize);

    gle::defvertex();

    gle::begin(GL_LINES);
    gle::attrib(e->o);
    gle::attrib(target);
    xtraverts += gle::end();

    gle::begin(GL_TRIANGLE_FAN);
    gle::attrib(target);
    loopi(5) gle::attrib(vec(spoke).rotate(2*M_PI*i/4.0f, dir).add(arrowbase));
    xtraverts += gle::end();
}

void renderentcone(const entities::classes::CoreEntity *e, const vec &dir, float radius, float angle)
{
    if(radius <= 0) return;
    vec spot = vec(dir).mul(radius*cosf(angle*RAD)).add(e->o), spoke;
    spoke.orthogonal(dir);
    spoke.normalize();
    spoke.mul(radius*sinf(angle*RAD));

    gle::defvertex();

    gle::begin(GL_LINES);
    loopi(8)
    {
        gle::attrib(e->o);
        gle::attrib(vec(spoke).rotate(2*M_PI*i/8.0f, dir).add(spot));
    }
    xtraverts += gle::end();

    gle::begin(GL_LINE_LOOP);
    loopi(8) gle::attrib(vec(spoke).rotate(2*M_PI*i/8.0f, dir).add(spot));
    xtraverts += gle::end();
}

void renderentbox(const entities::classes::CoreEntity *e, const vec &center, const vec &radius, int yaw, int pitch, int roll)
{
    matrix4x3 orient;
    orient.identity();
    orient.settranslation(e->o);
    if(yaw) orient.rotate_around_z(sincosmod360(yaw));
    if(pitch) orient.rotate_around_x(sincosmod360(pitch));
    if(roll) orient.rotate_around_y(sincosmod360(-roll));
    orient.translate(center);

    gle::defvertex();

    vec front[4] = { vec(-radius.x, -radius.y, -radius.z), vec( radius.x, -radius.y, -radius.z), vec( radius.x, -radius.y,  radius.z), vec(-radius.x, -radius.y,  radius.z) },
        back[4] = { vec(-radius.x, radius.y, -radius.z), vec( radius.x, radius.y, -radius.z), vec( radius.x, radius.y,  radius.z), vec(-radius.x, radius.y,  radius.z) };
    loopi(4)
    {
        front[i] = orient.transform(front[i]);
        back[i] = orient.transform(back[i]);
    }

    gle::begin(GL_LINE_LOOP);
    loopi(4) gle::attrib(front[i]);
    xtraverts += gle::end();

    gle::begin(GL_LINES);
    gle::attrib(front[0]);
        gle::attrib(front[2]);
    gle::attrib(front[1]);
        gle::attrib(front[3]);
    loopi(4)
    {
        gle::attrib(front[i]);
        gle::attrib(back[i]);
    }
    xtraverts += gle::end();

    gle::begin(GL_LINE_LOOP);
    loopi(4) gle::attrib(back[i]);
    xtraverts += gle::end();
}

void renderentradius(entities::classes::CoreEntity *e, bool color)
{
    switch(e->et_type)
    {
        case ET_LIGHT:
            if(e->attr1 <= 0) break;
            if(color) gle::colorf(e->attr2/255.0f, e->attr3/255.0f, e->attr4/255.0f);
            renderentsphere(e, e->attr1);
            break;

        case ET_SPOTLIGHT:
            if(e->attached)
            {
                if(color) gle::colorf(0, 1, 1);
                float radius = e->attached->attr1;
                if(radius <= 0) break;
                vec dir = vec(e->o).sub(e->attached->o).normalize();
                float angle = clamp(int(e->attr1), 1, 89);
                renderentattachment(e);
                renderentcone(e->attached, dir, radius, angle);
            }
            break;

        case ET_SOUND:
            if(color) gle::colorf(0, 1, 1);
            renderentsphere(e, e->attr2);
            break;

        case ET_ENVMAP:
        {
            extern int envmapradius;
            if(color) gle::colorf(0, 1, 1);
            renderentsphere(e, e->attr1 ? max(0, min(10000, int(e->attr1))) : envmapradius);
            break;
        }

        case ET_MAPMODEL:
        {
            if(color) gle::colorf(0, 1, 1);
            entities::entradius(e, color);
            vec dir;
            vecfromyawpitch(e->attr2, e->attr3, 1, 0, dir);
            renderentarrow(e, dir, 4);
            break;
        }

        case ET_PLAYERSTART:
        {
            if(color) gle::colorf(0, 1, 1);
            entities::entradius(e, color);
            vec dir;
            vecfromyawpitch(e->attr1, 0, 1, 0, dir);
            renderentarrow(e, dir, 4);
            break;
        }

        case ET_DECAL:
        {
            if(color) gle::colorf(0, 1, 1);
            DecalSlot &s = lookupdecalslot(e->attr1, false);
            float size = max(float(e->attr5), 1.0f);
            renderentbox(e, vec(0, s.depth * size/2, 0), vec(size/2, s.depth * size/2, size/2), e->attr2, e->attr3, e->attr4);
            break;
        }

        default:
            if(e->et_type>=ET_GAMESPECIFIC)
            {
                if(color) gle::colorf(0, 1, 1);
                entities::entradius(e, color);
            }
            break;
    }
}

static void renderentbox(const vec &eo, vec es)
{
    es.add(eo);

    // bottom quad
    gle::attrib(eo.x, eo.y, eo.z); gle::attrib(es.x, eo.y, eo.z);
    gle::attrib(es.x, eo.y, eo.z); gle::attrib(es.x, es.y, eo.z);
    gle::attrib(es.x, es.y, eo.z); gle::attrib(eo.x, es.y, eo.z);
    gle::attrib(eo.x, es.y, eo.z); gle::attrib(eo.x, eo.y, eo.z);

    // top quad
    gle::attrib(eo.x, eo.y, es.z); gle::attrib(es.x, eo.y, es.z);
    gle::attrib(es.x, eo.y, es.z); gle::attrib(es.x, es.y, es.z);
    gle::attrib(es.x, es.y, es.z); gle::attrib(eo.x, es.y, es.z);
    gle::attrib(eo.x, es.y, es.z); gle::attrib(eo.x, eo.y, es.z);

    // sides
    gle::attrib(eo.x, eo.y, eo.z); gle::attrib(eo.x, eo.y, es.z);
    gle::attrib(es.x, eo.y, eo.z); gle::attrib(es.x, eo.y, es.z);
    gle::attrib(es.x, es.y, eo.z); gle::attrib(es.x, es.y, es.z);
    gle::attrib(eo.x, es.y, eo.z); gle::attrib(eo.x, es.y, es.z);
}

void renderentselection(const vec &o, const vec &ray, bool entmoving)
{
    if(noentedit() || (entgroup.empty() && enthover < 0)) return;
    vec eo, es;

    if(entgroup.length())
    {
        gle::colorub(0, 40, 0);
        gle::defvertex();
        gle::begin(GL_LINES, entgroup.length()*24);
        loopv(entgroup) entfocus(entgroup[i],
            entselectionbox(e, eo, es);
            renderentbox(eo, es);
        );
        xtraverts += gle::end();
    }

    if(enthover >= 0 && enthover < entities::getents().length())
    {
		auto highlighted_ent = entities::getents()[enthover];
		float cameraRelativeTickness = clamp(0.015f*camera1->o.dist(highlighted_ent->o)*tan(fovy*0.5f*RAD), 0.1f, 1.0f);
		highlighted_ent->renderHighlight(entselradius, entorient, cameraRelativeTickness);

        if(entmoving && entmovingshadow==1)
        {
			highlighted_ent->renderMoveShadow(entselradius, worldsize);
        }
    }

    if(showentradius)
    {
        glDepthFunc(GL_GREATER);
        gle::colorf(0.25f, 0.25f, 0.25f);
        loopv(entgroup) entfocus(entgroup[i], renderentradius(e, false));
        if(enthover>=0) entfocus(enthover, renderentradius(e, false));
        glDepthFunc(GL_LESS);
        loopv(entgroup) entfocus(entgroup[i], renderentradius(e, true));
        if(enthover>=0) entfocus(enthover, renderentradius(e, true));
    }
}

bool enttoggle(int id)
{
    undonext = true;
    int i = entgroup.find(id);
    if(i < 0)
        entadd(id);
    else
        entgroup.remove(i);
    return i < 0;
}

bool hoveringonent(int ent, int orient)
{
    if(noentedit()) return false;
    entorient = orient;
    if((efocus = enthover = ent) >= 0)
        return true;
    efocus   = entgroup.empty() ? -1 : entgroup.last();
    enthover = -1;
    return false;
}

VAR(entitysurf, 0, 0, 1);

SCRIPTEXPORT_AS(entadd) void entadd_scriptimpl()
{
    if(enthover >= 0 && !noentedit())
    {
        if(entgroup.find(enthover) < 0) entadd(enthover);
        if(entmoving > 1) entmoving = 1;
    }
}

SCRIPTEXPORT_AS(enttoggle) void enttoggle_scriptimpl()
{
    if(enthover < 0 || noentedit() || !enttoggle(enthover)) { entmoving = 0; intret(0); }
    else { if(entmoving > 1) entmoving = 1; intret(1); }
}

SCRIPTEXPORT_AS(entmoving) void entmoving_scriptimpl(CommandTypes::Boolean n)
{
    if(*n >= 0)
    {
        if(!*n || enthover < 0 || noentedit()) entmoving = 0;
        else
        {
            if(entgroup.find(enthover) < 0) { entadd(enthover); entmoving = 1; }
            else if(!entmoving) entmoving = 1;
        }
    }
    intret(entmoving);
}

SCRIPTEXPORT void entpush(int *dir)
{
    if(noentedit()) return;
    int d = dimension(entorient);
    int s = dimcoord(entorient) ? -*dir : *dir;
    if(entmoving)
    {
        groupeditpure(e->o[d] += float(s*sel.grid)); // editdrag supplies the undo
    }
    else
        groupedit(e->o[d] += float(s*sel.grid));
    if(entitysurf==1)
    {
        player->o[d] += float(s*sel.grid);
        player->resetinterp();
    }
}

VAR(entautoviewdist, 0, 25, 100);
SCRIPTEXPORT void entautoview(int *dir)
{
    if(!haveselent()) return;
    static int s = 0;
    vec v(player->o);
    v.sub(worldpos);
    v.normalize();
    v.mul(entautoviewdist);
    int t = s + *dir;
    s = abs(t) % entgroup.length();
    if(t<0 && s>0) s = entgroup.length() - s;
    entfocus(entgroup[s],
        v.add(e->o);
        player->o = v;
        player->resetinterp();
    );
}


SCRIPTEXPORT void delent()
{
    if(noentedit()) return;
    groupedit(e->et_type = ET_EMPTY;);
    entcancel();
}

int findtype(char *what)
{
    for(int i = 0; *entities::entname(i); i++) if(strcmp(what, entities::entname(i))==0) return i;
    conoutf(CON_ERROR, "unknown entity type \"%s\"", what);
    return ET_EMPTY;
}

VAR(entdrop, 0, 2, 3);

bool dropentity(entities::classes::CoreEntity *e, int drop = -1)
{
    if (!e) {
        return false;
    }

    vec radius(4.0f, 4.0f, 4.0f);
    if(drop<0) drop = entdrop;
    if(e->et_type == ET_MAPMODEL)
    {
        model *m = loadmapmodel(e->model_idx);
        if(m)
        {
            vec center;
            mmboundbox(e, m, center, radius);
            radius.x += fabs(center.x);
            radius.y += fabs(center.y);
        }
        radius.z = 0.0f;
    }
    switch(drop)
    {
    case 1:
        if(e->et_type != ET_LIGHT && e->et_type != ET_SPOTLIGHT)
           dropenttofloor(e);
        break;
    case 2:
    case 3:
        int cx = 0, cy = 0;
        if(sel.cxs == 1 && sel.cys == 1)
        {
            cx = (sel.cx ? 1 : -1) * sel.grid / 2;
            cy = (sel.cy ? 1 : -1) * sel.grid / 2;
        }
        e->o = vec(sel.o);
        int d = dimension(sel.orient), dc = dimcoord(sel.orient);
        e->o[R[d]] += sel.grid / 2 + cx;
        e->o[C[d]] += sel.grid / 2 + cy;
        if(!dc)
            e->o[D[d]] -= radius[D[d]];
        else
            e->o[D[d]] += sel.grid + radius[D[d]];

        if(e->et_type != ET_LIGHT && e->et_type != ET_SPOTLIGHT)
            dropenttofloor(e);
        break;
    }
    return true;
}

SCRIPTEXPORT void dropent()
{
    if(noentedit()) return;
    groupedit(dropentity(e));
}

SCRIPTEXPORT void attachent()
{
    if(noentedit()) return;
    groupedit(attachentity((entities::classes::BasePhysicalEntity*)&e));
}

static int keepents = 0;

entities::classes::CoreEntity *newentity(bool local, const vec &o, int type, int v1, int v2, int v3, int v4, int v5, int &idx, bool fix = true)
{
    auto &ents = entities::getents();

    // If local, we ensure it is not out of bounds, if it is, we return NULL and warn our player.
    if(local)
    {
        idx = -1;
        for(int i = keepents; i < ents.length(); i++)
        {
			if(ents[i]->et_type == ET_EMPTY)
			{
				idx = i; break;
			}
		}
        if(idx < 0 && ents.length() >= MAXENTS)
        {
			conoutf("too many entities");
			return NULL;
		}
    } else {
        while(ents.length() < idx)
        {
            ents.add(entities::newgameentity(""))->et_type = ET_EMPTY;
        }
    }

    auto e = entities::newgameentity("");
    e->o = o;
    e->attr1 = v1;
    e->attr2 = v2;
    e->attr3 = v3;
    e->attr4 = v4;
    e->attr5 = v5;
    e->et_type = type;
    e->ent_type = ENT_INANIMATE;
    e->game_type = type;
    e->reserved = 0;
	e->name = "tesseract_ent_" + std::to_string(idx);

    if(ents.inrange(idx))
    {
		entities::deletegameentity(ents[idx]);
		ents[idx] = e;
	}
    else
    {
		idx = ents.length();
		ents.add(e);
	}
	return e;
}

void newentity(int type, int a1, int a2, int a3, int a4, int a5, bool fix = true)
{
    int idx;
    auto t = newentity(true, player->o, type, a1, a2, a3, a4, a5, idx, fix);
    if(!t) return;
    dropentity(t);
    t->et_type = ET_EMPTY;
    enttoggle(idx);
    makeundoent();
    entedit(idx, e->et_type = type);
    commitchanges();
}

SCRIPTEXPORT void newent(char *what, int *a1, int *a2, int *a3, int *a4, int *a5)
{
    if(noentedit()) return;
    int type = findtype(what);
    if(type != ET_EMPTY)
        newentity(type, *a1, *a2, *a3, *a4, *a5);
}

// WatIs: Game entity creation.
#include "../game/game.h"

entities::classes::CoreEntity *new_game_entity(bool local, const vec &o, int &idx, const char *strclass)
{
    // Retreive the list of entities.
    auto &ents = entities::getents();

    // If local, we ensure it is not out of bounds, if it is, we return NULL and warn our player.
    if(local)
    {
        idx = -1;
        for(int i = keepents; i < ents.length(); i++)
        {
            if(ents[i]->et_type == ET_EMPTY)
            {
                conoutf("idx = %i", idx);
                idx = i;
                break;
            }
        }
        if(idx < 0 && ents.length() >= MAXENTS)
        {
            conoutf("too many entities"); return nullptr;
        }
    } else {
        while(ents.length() < idx) {
//            ents.add(entities::newgameentity(strclass))->et_type = ET_EMPTY;
			ents.add(entities::newgameentity(""))->et_type = ET_EMPTY;
        }
    }

	// Allocate the entity according to its class.
    entities::classes::CoreEntity *ent = entities::newgameentity(strclass);

	// Set origin.
    ent->o = o;

	// Set entity type, it is now aware that it is a game specific entity. (Thus based on class name.)
    ent->et_type = ET_GAMESPECIFIC;

	// Set internal engine entity type, ENT_PLAYER, ENT_INANIMATE, ENT_AI etc.
    ent->ent_type = ENT_INANIMATE;

	// This one is a bit of a....
    ent->game_type = GAMEENTITY;

	if(ents.inrange(idx)) {
		entities::deletegameentity(ents[idx]);
        ents[idx] = ent;
	} else {
		idx = ents.length();
        ents.add(ent);
	}
	
	auto new_et_type = ent->et_type;
	auto new_ent_type = ent->ent_type;
	auto new_game_type = ent->game_type;

	enttoggle(idx);
	makeundoent();
	entedit(idx, e->et_type = new_et_type; e->ent_type = new_ent_type; e->game_type = new_game_type);
	commitchanges();
	
    return ent;
}

// Start of new game entity.
void new_game_entity(char *strclass, char *a1, char *a2, char *a3, char *a4, char *a5, char *a6, char *a7, char *a8, bool fix = true)
{
    int idx;
    entities::classes::CoreEntity *t = new_game_entity(true, player->o, idx, strclass);
    if(!t) return;
    dropentity(t);
    //t->et_type = ET_EMPTY; // Why would we want this here if we set e->type later
    int new_et_type = t->et_type;
    int new_ent_type = t->ent_type;
    int new_game_type = t->game_type;
    t->et_type = new_et_type;
    t->ent_type = new_ent_type;
    t->game_type = new_game_type;

    // Copy string attributes.
	t->name = std::string(strclass) + "_" + std::to_string(idx);

    enttoggle(idx);
    makeundoent();
    entedit(idx, e->et_type = new_et_type; e->ent_type = new_ent_type; e->game_type = new_game_type);
    commitchanges();
}

SCRIPTEXPORT void newgent(char *what, char *a1, char *a2, char *a3, char *a4, char *a5, char *a6, char *a7, char *a8)
{
    // TODO: Determine what "what" is, and use this as the entity type?
    // From there on, we modify newentity(the one in world.cpp, which was name colliding with newgameentity before I renamed it.)
    // Newentity will then pass the WHAT, over to newgameentity.
    //
    // From there on, we heavily need to modify the code. After all, this entity system is rather shitty.
    // Maybe first fix name confusions, gameent and gameentity... wow...
    //
    // TODO: Explain more here.
    new_game_entity(what, a1, a2, a3, a4, a5, a6, a7, a8);
}
// WatIs: End of new game entity.

int entcopygrid;
vector<entities::classes::CoreEntity*> entcopybuf;

SCRIPTEXPORT void entcopy()
{
    if(noentedit()) return;
    entcopygrid = sel.grid;
    entcopybuf.shrink(0);
    addimplicit({
        loopv(entgroup) entfocus(entgroup[i], entcopybuf.add(e)->o.sub(vec(sel.o)));
    });
}

SCRIPTEXPORT void entpaste()
{
    if(noentedit() || entcopybuf.empty()) return;
    entcancel();
    float m = float(sel.grid)/float(entcopygrid);
    loopv(entcopybuf)
    {
        const auto c = entcopybuf[i];
        vec o = vec(c->o).mul(m).add(vec(sel.o));
        int idx;
        entities::classes::CoreEntity *e = newentity(true, o, ET_EMPTY, c->attr1, c->attr2, c->attr3, c->attr4, c->attr5, idx);
        if(!e) continue;
        entadd(idx);
        keepents = max(keepents, idx+1);
    }
    keepents = 0;
    int j = 0;
    groupeditundo(e->et_type = entcopybuf[j++]->et_type;);
}

SCRIPTEXPORT void entreplace()
{
    if(noentedit() || entcopybuf.empty()) return;
    const auto c = entcopybuf[0];
    if(entgroup.length() || enthover >= 0)
    {
        groupedit({
            e->et_type = c->et_type;
            e->ent_type = c->ent_type;
            e->game_type = c->game_type;
            e->attr1 = c->attr1;
            e->attr2 = c->attr2;
            e->attr3 = c->attr3;
            e->attr4 = c->attr4;
            e->attr5 = c->attr5;
            e->model_idx = c->model_idx;
            e->name = c->name;
        });
    }
    else
    {
        newentity(c->et_type, c->attr1, c->attr2, c->attr3, c->attr4, c->attr5, false);
    }
}

SCRIPTEXPORT void entset(char *what, int *a1, int *a2, int *a3, int *a4, int *a5)
{
    if(noentedit()) return;
    int type = findtype(what);
    if(type != ET_EMPTY)
        groupedit(e->et_type=type;
                  e->attr1=*a1;
                  e->attr2=*a2;
                  e->attr3=*a3;
                  e->attr4=*a4;
                  e->attr5=*a5);
}

void printent(entities::classes::CoreEntity *e, char *buf, int len)
{
    switch(e->et_type)
    {
        case ET_PARTICLES:
            if(printparticles(e, buf, len)) return;
            break;

        default:
            if(e->et_type >= ET_GAMESPECIFIC && entities::printent(e, buf, len)) return;
            break;
    }
    nformatcubestr(buf, len, "%s %d %d %d %d %d", entities::entname(e->et_type), e->attr1, e->attr2, e->attr3, e->attr4, e->attr5);
}

SCRIPTEXPORT void nearestent()
{
    if(noentedit()) return;
    int closest = -1;
    float closedist = 1e16f;
    auto &ents = entities::getents();
    loopv(ents)
    {
        entities::classes::CoreEntity *e = ents[i];
        if(e->et_type == ET_EMPTY) continue;
        float dist = e->o.dist(player->o);
        if(dist < closedist)
        {
            closest = i;
            closedist = dist;
        }
    }
    if(closest >= 0) entadd(closest);
}

SCRIPTEXPORT void enthavesel()
{
    addimplicit(intret(entgroup.length()));
}

SCRIPTEXPORT void entselect(CommandTypes::Expression body)
{
    if(!noentedit()) addgroup(e->et_type != ET_EMPTY && entgroup.find(n)<0 && executebool(body));
}

SCRIPTEXPORT void entloop(CommandTypes::Expression body)
{
    if(!noentedit()) addimplicit(groupeditloop(((void)e, execute(body))));
}

SCRIPTEXPORT void insel()
{
    entfocus(efocus, intret(pointinsel(sel, e->o)));
}

SCRIPTEXPORT void entget()
{
    entfocus(efocus, cubestr s; printent(e, s, sizeof(s)); result(s));
}

SCRIPTEXPORT void entindex()
{
    intret(efocus);
}

SCRIPTEXPORT void enttype(char *type, CommandTypes::ArgLen numargs)
{
    if(*numargs >= 1)
    {
        int typeidx = findtype(type);
        if(typeidx != ET_EMPTY) groupedit(e->et_type = typeidx);
    }
    else entfocus(efocus,
    {
        result(entities::entname(e->et_type));
    })
}

SCRIPTEXPORT void entattr(int *attr, int *val, CommandTypes::ArgLen numargs)
{
    if(*numargs >= 2)
    {
        if(*attr >= 0 && *attr <= 4)
            groupedit(
                switch(*attr)
                {
                    case 0: e->attr1 = *val; break;
                    case 1: e->attr2 = *val; break;
                    case 2: e->attr3 = *val; break;
                    case 3: e->attr4 = *val; break;
                    case 4: e->attr5 = *val; break;
                }
            )
    }
    else entfocus(efocus,
    {
        switch(*attr)
        {
            case 0: intret(e->attr1); break;
            case 1: intret(e->attr2); break;
            case 2: intret(e->attr3); break;
            case 3: intret(e->attr4); break;
            case 4: intret(e->attr5); break;
        }
    })
}

// TODO: Is this still needed?
int findentity(int type, int index, int attr1, int attr2)
{
    const auto &ents = entities::getents();
    if(index > ents.length()) index = ents.length();
    else for(int i = index; i<ents.length(); i++)
    {
        entities::classes::CoreEntity *e = ents[i];
        if (e->et_type == ET_MAPMODEL && (attr1 < 0 || e->model_idx == attr1) && (attr2 < 0 || e->attr2 == attr2))
            return i;
        if(e->et_type==type && (attr1<0 || e->attr1==attr1) && (attr2<0 || e->attr2==attr2))
            return i;
    }
    loopj(index)
    {
        entities::classes::CoreEntity *e = ents[j];
        if (e->et_type == ET_MAPMODEL && (attr1 < 0 || e->model_idx == attr1) && (attr2 < 0 || e->attr2 == attr2))
            return j;
        if(e->et_type==type && (attr1<0 || e->attr1==attr1) && (attr2<0 || e->attr2==attr2))
            return j;
    }
    return -1;
}

int findentity_byclass(const std::string &classname)
{
	const auto &ents = entities::getents();
	for(int i = 0; i <ents.length(); i++)
	{
		if (ents[i]->classname != classname) continue;
		
		return i;
	}

	return -1;
}


// We do not need forceent = -1 anymore atm, neither do we need tag = 0 for now. But it's here for backwards reasons.
void findplayerspawn(entities::classes::Player *d, int forceent, int tag) // Place at spawn (some day, random spawn).
{
	auto startEntity = getentitybytype<entities::classes::PlayerStart>();

	if (startEntity)
	{
		d->o = startEntity->o;
		d->o.z += 1;
		d->yaw = startEntity->yaw;
	}
	else
	{
		conoutf(CON_WARN, "Unable to find a PlayerStart, defaulting to mapcenter.. ");
		
		d->o.x = d->o.y = d->o.z = 0.5f*worldsize;
		d->o.z += 1;
		d->yaw = 0.0f;
	}
	
	d->resetinterp();
	entinmap(d);
}

//=====================
// WatIsDeze: Old findplayerspawn codes. We don't need these for now.
//
//=====================
//int spawncycle = -1;

//void findplayerspawn(dynent *d, int forceent, int tag) // place at random spawn
//{
//    int pick = forceent;
//    if(pick<0)
//    {
//        int r = rnd(10)+1;
//        pick = spawncycle;
//        loopi(r)
//        {
//            pick = findentity(ET_PLAYERSTART, pick+1, -1, tag);
//            if(pick < 0) break;
//        }
//        if(pick < 0 && tag)
//        {
//            pick = spawncycle;
//            loopi(r)
//            {
//                pick = findentity(ET_PLAYERSTART, pick+1, -1, 0);
//                if(pick < 0) break;
//            }
//        }
//        if(pick >= 0) spawncycle = pick;
//    }
//    if(pick>=0)
//    {
//        const vector<extentity *> &ents = entities::getents();
//        d->pitch = 0;
//        d->roll = 0;
//        for(int attempt = pick;;)
//        {
//            d->o = ents[attempt]->o;
//            d->yaw = ents[attempt]->attr1;
//            if(entinmap(d, true)) break;
//            attempt = findentity(ET_PLAYERSTART, attempt+1, -1, tag);
//            if(attempt<0 || attempt==pick)
//            {
//                d->o = ents[pick]->o;
//                d->yaw = ents[pick]->attr1;
//                entinmap(d);
//                break;
//            }
//        }
//    }
//    else
//    {
//        d->o.x = d->o.y = d->o.z = 0.5f*worldsize;
//        d->o.z += 1;
//        entinmap(d);
//    }
//}
//int findentity_byclass(const std::string &classname, int index, int attr1, int attr2)
//{
//    const auto &ents = entities::getents();
//    if(index > ents.length()) index = ents.length();
//    else {
//        for(int i = index; i<ents.length(); i++)
//        {
//            entities::classes::CoreEntity *e = ents[i];

//            if(e->classname == classname) {
//				conoutf("Found Entity by Class: %s , %s", classname.c_str(), e->classname.c_str());
//                return i;
//            }
//        }
//    }

//    return index;
//}

//void findplayerspawn(entities::classes::Player *d, int forceent, int tag) // place at random spawn
//{
//	int pick = forceent;
//	if(pick < 0)
//	{
//		int r = rnd(10)+1;
//		pick = spawncycle;
//		loopi(r)
//		{
//			pick = findentity_byclass("playerstart", pick+1, -1, tag);
//			if(pick < 0) break;
//		}
//		if(pick < 0 && tag)
//		{
//			pick = spawncycle;
//			loopi(r)
//			{
//				pick = findentity_byclass("playerstart", pick+1, -1, 0);
//				if(pick < 0) break;
//			}
//		}
//		if(pick >= 0) spawncycle = pick;
//	}
//	if(pick>=0)
//	{
//		const auto &ents = entities::getents();
//		d->pitch = 0;
//		d->roll = 0;
//		for(int attempt = pick; attempt < ents.length(); attempt++ )
//		{
//			d->o = ents[attempt]->o;
//			d->yaw = ents[attempt]->attr1;
//			if(entinmap(d, true)) break;
//			attempt = findentity_byclass("playerstart", attempt+1, -1, tag);
//			if(attempt < 0 || attempt==pick)
//			{
//				d->o = ents[pick]->o;
//				d->yaw = ents[pick]->attr1;
//				entinmap(d);
//				break;
//			}
//		}
//	}
//	else
//	{
//        d->o.x = d->o.y = d->o.z = 0.5f*worldsize;
//        d->o.z += 1;
//        entinmap(d);
//	}
//}

void splitocta(cube *c, int size)
{
    if(size <= 0x1000) return;
    loopi(8)
    {
        if(!c[i].children) c[i].children = newcubes(isempty(c[i]) ? F_EMPTY : F_SOLID);
        splitocta(c[i].children, size>>1);
    }
}

void resetmap()
{
    clearoverrides();
    clearmapsounds();
    resetblendmap();
    clearlights();
    clearpvs();
    clearslots();
    clearparticles();
    clearstains();
    clearsleep();
    cancelsel();
    pruneundos();
    clearmapcrc();

    entities::clearents();
    outsideents.setsize(0);
    spotlights = 0;
    volumetriclights = 0;
    nospeclights = 0;
}

void startmap(const char *name)
{
    game::startmap(name);
}

bool emptymap(int scale, bool force, const char *mname, bool usecfg)    // main empty world creation routine
{
    if(!force && !editmode)
    {
        conoutf(CON_ERROR, "newmap only allowed in edit mode");
        return false;
    }

    logoutf("reset map");
    resetmap();

    setvar("mapscale", scale<10 ? 10 : (scale>16 ? 16 : scale), true, false);
    setvar("mapsize", 1<<worldscale, true, false);
    setvar("emptymap", 1, true, false);

    texmru.shrink(0);
    logoutf("freeocta worldroot1");
    freeocta(worldroot);
    worldroot = newcubes(F_EMPTY);
    loopi(4) solidfaces(worldroot[i]);
    logoutf("worldroot = new cubes(F_EMPTY)");

    if(worldsize > 0x1000) splitocta(worldroot, worldsize>>1);

    clearmainmenu();
    logoutf("clearmenu");

    if(usecfg)
    {
        identflags |= IDF_OVERRIDDEN;
        execfile("config/default_map_settings.cfg", false);
        identflags &= ~IDF_OVERRIDDEN;
    }

    allchanged(true);
    logoutf("allchanged true");

    logoutf("startmap %s", mname);
    startmap(mname);

    return true;
}

bool enlargemap(bool force)
{
    if(!force && !editmode)
    {
        conoutf(CON_ERROR, "mapenlarge only allowed in edit mode");
        return false;
    }
    if(worldsize >= 1<<16) return false;

    while(outsideents.length()) removeentity(outsideents.pop());

    worldscale++;
    worldsize *= 2;
    cube *c = newcubes(F_EMPTY);
    c[0].children = worldroot;
    loopi(3) solidfaces(c[i+1]);
    worldroot = c;

    if(worldsize > 0x1000) splitocta(worldroot, worldsize>>1);

    enlargeblendmap();

    allchanged();

    return true;
}

static bool isallempty(cube &c)
{
    if(!c.children) return isempty(c);
    loopi(8) if(!isallempty(c.children[i])) return false;
    return true;
}

SCRIPTEXPORT void shrinkmap()
{
    extern int nompedit;
    if(noedit(true) || (nompedit && multiplayer())) return;
    if(worldsize <= 1<<10) return;

    int octant = -1;
    loopi(8) if(!isallempty(worldroot[i]))
    {
        if(octant >= 0) return;
        octant = i;
    }
    if(octant < 0) return;

    while(outsideents.length()) removeentity(outsideents.pop());

    if(!worldroot[octant].children) subdividecube(worldroot[octant], false, false);
    cube *root = worldroot[octant].children;
    worldroot[octant].children = NULL;
    freeocta(worldroot);
    worldroot = root;
    worldscale--;
    worldsize /= 2;

    ivec offset(octant, ivec(0, 0, 0), worldsize);
    auto &ents = entities::getents();
    loopv(ents) ents[i]->o.sub(vec(offset));

    shrinkblendmap(octant);

    allchanged();

    conoutf("shrunk map to size %d", worldscale);
}

SCRIPTEXPORT void newmap(int *i) { bool force = !isconnected(); if(force) game::forceedit(""); if(emptymap(*i, force, NULL)) game::newmap(max(*i, 0)); }
SCRIPTEXPORT void mapenlarge() { if(enlargemap(false)) game::newmap(-1); }

SCRIPTEXPORT void mapname()
{
    result(game::getclientmap());
}

void mpeditent(int i, const vec &o, int type, int attr1, int attr2, int attr3, int attr4, int attr5, bool local)
{
    if(i < 0 || i >= MAXENTS) return;
    auto &ents = entities::getents();
    if(ents.length()<=i)
    {
        entities::classes::CoreEntity *e = newentity(local, o, type, attr1, attr2, attr3, attr4, attr5, i);
        if(!e) return;
        addentityedit(i);
        attachentity(e);
    }
    else
    {
        entities::classes::CoreEntity *e = ents[i];
        removeentityedit(i);
        int old_et_type = e->et_type;
        int old_ent_type = e->ent_type;
        int old_game_type = e->game_type;
        if(old_et_type!=type) detachentity(e);
        e->et_type = type;
        e->ent_type = old_ent_type;
        e->game_type = old_game_type;
        e->o = o;
        e->attr1 = attr1; e->attr2 = attr2; e->attr3 = attr3; e->attr4 = attr4; e->attr5 = attr5;
        if (e->et_type == ET_MAPMODEL)
            e->model_idx = attr1;
        else
            e->model_idx = -1;

        addentityedit(i);
        if(old_et_type!=type) attachentity(e);
    }
    entities::editent(i, local);
    clearshadowcache();
    commitchanges();
}

int getworldsize() { return worldsize; }
int getmapversion() { return mapversion; }


// >>>>>>>>>> SCRIPTBIND >>>>>>>>>>>>>> //
#if 0
#include "/Users/micha/dev/ScMaMike/src/build/binding/..+engine+world.binding.cpp"
#endif
// <<<<<<<<<< SCRIPTBIND <<<<<<<<<<<<<< //
