#include "game.h"
#include "engine/scriptexport.h"
#include "shared/entities/basecliententity.h"
#include "shared/entities/baseentity.h"
#include "game/entities.h"
#include "game/entities/player.h"

namespace game
{
    //__attribute__((optimize("O0"))) void 
    void RenderGameEntities()
    {
        loopv(entities::getents()) {
            entities::classes::BaseEntity *ent = dynamic_cast<entities::classes::BaseEntity*>(entities::getents()[i]);
            //if (ent->et_type != ET_PLAYERSTART && ent->et_type != ET_EMPTY && ent->et_type != ET_LIGHT && ent->et_type != ET_SPOTLIGHT && ent->et_type != ET_SOUND)

            // Ensure we only render player entities if it isn't our own player 1 entity. (Otherwise we'd render it double.)
            if (ent != nullptr && (ent != game::player1))
                ent->render();
        }

        // Render our client player.
        if (game::player1 != nullptr)
            game::player1->render();
    }

    VARP(hudgun, 0, 1, 1);
    VARP(hudgunsway, 0, 1, 1);

    FVAR(swaystep, 1, 35.0f, 100);
    FVAR(swayside, 0, 0.10f, 1);
    FVAR(swayup, -1, 0.15f, 1);

    float swayfade = 0, swayspeed = 0, swaydist = 0;
    vec swaydir(0, 0, 0);

    void swayhudgun(int curtime) {

    }


    void drawhudmodel(entities::classes::CoreEntity *d, int anim, int basetime) {

    }

    void drawhudgun() {

    }

    void renderplayerpreview(int model, int color, int team, int weap) {

    }

    vec hudgunorigin(int gun, const vec &from, const vec &to, entities::classes::CoreEntity *d) {
        vec offset(from);

        return offset;
    }

    int numanims() {
        return 0;
    }

    void findanims(const char *pattern, vector<int> &anims)
    {
        //loopi(sizeof(animnames)/sizeof(animnames[0])) if(matchanim(animnames[i], pattern)) anims.add(i);
    }

    void preloadweapons() {

    }

    void preloadsounds()
    {

    }

    int getplayercolor(int team, int color)
    {
        switch(team)
        {
            case 1: return 0x0000FF;
            case 2: return 0xFF0000;
            default: return 0xFFFF77;
        }
    }

    int _getplayercolor(entities::classes::BaseClientEntity *d, int color) {
        if (!d || !game::players.inrange(d->ci.clientNumber)) {}
            conoutf("Client index %i does not exist, can't retrieve color", d->ci->clientNumber);
            return getplayercolor(d->ci.clientNumber, color);
    }
    
    SCRIPTEXPORT_AS(getclientcolor) void GetClientColor(int *cn, int *color) {
        intret(_getplayercolor(dynamic_cast<entities::classes::BaseClientEntity*>(game::GetClient(*cn)), *color));
    }
}   
// >>>>>>>>>> SCRIPTBIND >>>>>>>>>>>>>> //
#if 0
#include "/Users/micha/dev/ScMaMike/src/build/binding/..+game+render.binding.cpp"
#endif
// <<<<<<<<<< SCRIPTBIND <<<<<<<<<<<<<< //
