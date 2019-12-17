#include "engine/engine.h"
#include "engine/scriptexport.h"

#include "shared/networking/network.h"
#include "shared/networking/protocol.h"
#include "shared/networking/cl_sv.h"
#include "shared/networking/frametimestate.h"
#include "shared/entities/coreentity.h"
#include "shared/entities/basedynamicentity.h"

#include "game/game.h"
#include "game/client/client.h"
#include "game/server/server.h"
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  
#include "game/entities.h"
#include "game/entities/player.h"
#include "game/entities/playerstart.h"

namespace game
{
    // Stores all the player entities that are in our (local, or remotte) server.
    vector<entities::classes::BaseClientEntity *> players;
    networking::protocol::MasterMode masterMode = networking::protocol::MasterMode::Private; // Current game mode (M_LOBBY, M_EDIT | M_LOCAL etc)
    networking::GameMode gameMode = networking::GameMode::Edit; // Master Privilige mode (when hosting with this client).

    bool sendItemsToServer = false, sendCRC = false; // after a map change, since server doesn't have map data
    int lastPing = 0;

   // Networking State properties.
    bool connected = false, remote = false, demoPlayback = false, gamePaused = false;
    int sessionID = 0, gameSpeed = 100;
    cubestr servDesc = "", servAuth = "", connectPass = "";

    VARP(deadpush, 1, 2, 20);

    // Minimap radar.
    VARP(minradarscale, 0, 384, 10000);
    VARP(maxradarscale, 1, 1024, 10000);
    VARP(radarteammates, 0, 1, 1);
    FVARP(minimapalpha, 0, 1, 1);

    // Global player entity pointer.
    ::entities::classes::Player *player1 = NULL;

    // Map Game State properties.
    cubestr clientmap = "";     // Current map filename.
    int maptime = 0;            // Frame time.
    int maprealtime = 0;        // Total time.


    void switchname(const char *name)
    {
        std::string plName = name;

        if (game::player1->name.empty() || plName.empty()) { 
            game::player1->name = "untitled_" + std::to_string(game::player1->ci.clientNumber) + std::to_string(rnd(128));
        }

        game::client::AddMessage(game::networking::protocol::NetClientMessage::SwitchName, "rcs", game::player1->name.c_str());
    }

    ////////////////////
    // World functions
    ///////////////////
    void updateworld() {
        // Update the map time. (First frame since maptime = 0.
        if(!maptime) { 
            maptime = ftsClient.lastMilliseconds; 
            maprealtime = ftsClient.totalMilliseconds; 
            return; 
        }

        // Escape this function if there is no currenttime yet from server to client. (Meaning it is 0.)
        if(!ftsClient.currentTime) 
            return; 
            //{ gets2c(); if (player1->) c2sinfo(); return; } //c2sinfo(); }///if(player1->clientnum>=0) c2sinfo(); return; }
        //if(!curtime) return; //{ gets2c(); c2sinfo(); }///if(player1->clientnum>=0) c2sinfo(); return; }
        networking::GetS2C();

		// Update the physics.
        PhysicsFrame();

        // Update all our entity objects.
        updateentities();

        // Fetch server to client messages and info.
        networking::GetS2C();

        // Do this last, to reduce the effective frame lag
		if(player->ci.clientNumber >=0) 
            Client2ServerInfo();   
    }

    void SpawnPlayer()   // place at random spawn
    {
        if (player1)
        {
            player1->reset();
            player1->respawn();
        } else {
            player1 = new entities::classes::Player();
            player1->respawn();
        }
    }

    void updateentities() {
        // Execute think actions for entities.
        loopv(entities::getents())
        {
            // Let's go at it!
            if (entities::getents().inrange(i)) {
                entities::classes::BaseEntity *e = dynamic_cast<entities::classes::BaseEntity *>(entities::getents()[i]);
                if (e != NULL && e->ent_type != ENT_PLAYER)
                    e->think();
            }

        }

        if (game::player1)
            game::player1->think();
    }

    // Never seen an implementation of this function, should be part of BaseEntity.
    void dynentcollide(entities::classes::BaseDynamicEntity *d, entities::classes::BaseDynamicEntity *o, const vec &dir) {
        conoutf("dynentcollide D et_type: %i ent_type: %i game_type: %i --- O et_type: %i ent_type: %i game_type %i", d->et_type, d->ent_type, d->game_type, o->et_type, o->ent_type, o->game_type);
    }

    void mapmodelcollide(entities::classes::CoreEntity *d, entities::classes::CoreEntity *o, const vec &dir) {
        conoutf("mmcollide D et_type: %i ent_type: %i game_type: %i --- O et_type: %i ent_type: %i game_type %i", d->et_type, d->ent_type, d->game_type, o->et_type, o->ent_type, o->game_type);
    }

    // Never seen an implementation of this function, should be part of BaseEntity.
    void bounced(entities::classes::BasePhysicalEntity *d, const vec &surface) {}

    // Unsure what to do with these yet.
    void edittrigger(const selinfo &sel, int op, int arg1, int arg2, int arg3, const VSlot *vs) {

    }
    void vartrigger(ident *id) {

    }

    // These speak for themselves.
    const char *GetClientMap() {
        return clientMap;
    }
    const char *getmapinfo() {
        return NULL;
    }
    void resetgamestate() {
        //clearprojectiles();
        //clearbouncers();
    }
    void suicide(entities::classes::CoreEntity *d) {

    }

    void newmap(int size) {
        // Copy into mapname and reset maptime.
        maptime = 0;

        // Clear old entity list..
        entities::clearents();

        // SpawnPlayer.
        SpawnPlayer();

        // Find our playerspawn.
        findplayerspawn(game::player1);
    }

    void loadingmap(const char *name) {

    }

    void startmap(const char *name)
    {
        // Reset entity spawns.
		entities::resetspawns();

        // Spawn our player.
        SpawnPlayer();

		// Find player spawn point.
		findplayerspawn(game::player1);

        copycubestr(clientMap, name ? name : "");
        execident("mapstart");
    }

    bool needminimap() {
        return false;
    }

    float abovegameplayhud(int w, int h) {
        switch(player1->state)
        {
            case CS_EDITING:
                return 1;
            default:
                return (h-min(128, h))/float(h);
        }
    }

    void gameplayhud(int w, int h) {

    }

    float clipconsole(float w, float h) {
        return 0;
    }

    void preload() {
        entities::preloadentities();
    }

    void renderavatar() {

    }

    void renderplayerpreview(int model, int team, int weap) {
        //renderclient(player1, "actors/male", NULL, 1, 1, 4, 0, 0, 1.0f, false, 1);
    }

    bool canjump()
    {
        return player1->state!=CS_DEAD;
    }

    bool cancrouch()
    {
        return player1->state!=CS_DEAD;
    }

    bool allowmove(entities::classes::BasePhysicalEntity *d)
    {
        return true;
        //if(d->ent_type!=ENT_PLAYER) return true;
        //return !d->ms_lastaction || lastmillis-d->ms_lastaction>=1000;
    }

    entities::classes::CoreEntity *iterdynents(int i) {
        if (i == 0) {
            return player1;
        } else {
            if (i < entities::getents().length()) {
                return dynamic_cast<entities::classes::BaseEntity *>(entities::getents()[i]);
            } else {
                return nullptr;
            }
        }

        //if (i < entities::g_lightEnts.length()) return (entities::classes::BaseEntity*)entities::g_lightEnts[i];
        //    i -= entities::g_lightEnts.length();
        //return NULL;
    }
    // int numdynents() { return players.length()+monsters.length()+movables.length(); }
    int numdynents() {
        return entities::getents().length() + 1; // + 1 is for the player.
    }

    // This function should be used to render HUD View stuff etc.
    void rendergame(bool mainpass) {
        // This function should be used to render HUD View stuff etc.
    }

    const char *defaultcrosshair(int index) {
        return "data/crosshair.png";
    }

    int selectcrosshair(vec &color) {
        if(player1->state==CS_DEAD) return -1;
        return 0;
    }

    void setupcamera() {
        entities::classes::BasePhysicalEntity *target = dynamic_cast<entities::classes::BasePhysicalEntity*>(player1);
        assert(target);
        if(target)
        {
            player1->strafe = target->strafe;
            player1->move = target->move;
            player1->yaw = target->yaw;
            player1->pitch = target->state==CS_DEAD ? 0 : target->pitch;
            player1->o = target->o;
            player1->resetinterp();
        }
    }

    bool allowthirdperson() {
        return true;
    }

    bool detachcamera() {
        return player1->state==CS_DEAD;
    }

    bool collidecamera() {
        return player1->state!=CS_EDITING;
    }

    void lighteffects(entities::classes::CoreEntity *e, vec&color, vec &dir) {
    }

    void renderDynamicLights() {
        // Loop through our light entities and render them all.
        auto &ents = entities::getents();
        loopv(ents)
        {
            // Let's go at it!
            auto e = dynamic_cast<entities::classes::BaseEntity *>(ents[i]);
            if (!e || e->et_type != ET_LIGHT) continue;
            
            e->render();
        }
    }

    void dynlighttrack(entities::classes::CoreEntity *owner, vec &o, vec &hud) {
    }

    void particletrack(entities::classes::CoreEntity *owner, vec &o, vec &d) {
    }

    void writegamedata(vector<char> &extras) {

    }
    void readgamedata (vector<char> &extras) {

    }

    // Cpmfogiratopm fo;es/
    const char *GameCfg() { return "config/game.cfg"; }
    const char *SavedCfg() { return "config/saved.cfg"; }
    const char *DefaultCfg() { return "config/default.cfg"; }
    const char *AutoExecCfg() { return "config/autoexec.cfg"; }
    const char *SavedServersCfg() { return "config/servers.cfg"; }

    // Amy custom configuration files yo'd like to have loaded by default
    void loadconfigs() {
        execfile("config/auth.cfg", false);
    }

    void parseoptions(vector<const char *> &args) {
        //conoutf(CON_WARN, "game::parseoption is empty");
    }

    bool allowedittoggle() {
        if (!player1 || mainmenu == 1)
        {
            conoutf(CON_ERROR, "[%s:%i] %s", __FILE__, __LINE__, "Can't enter edit mode while in the mainmenu.");
            return false;
        }
//"[%s:%i] %s", __FILE__, __LINE__,
        return true; 
    }

    void edittoggled(bool on) {
        conoutf(CON_INFO, "Editor toggled - %s", (on == 1 ? "On" : "Off"));
    }


    void ConnectAttempt(const char *name, const char *password, const ENetAddress &address) {
        // Will need this to even join a game.
        //copycubestr(connectpass, password);
    }
    void ConnectFail() {}
    void ParsePacketclient(int chan, packetbuf &p) {}
    void writeclientinfo(stream *f) {}
    void toserver(char *text) {}
    bool ispaused() { return false; }
    int scaletime(int t) { return t*100; }
    bool allowmouselook() { return true; }

    //---------------------------------------------------------------

    void physicstrigger(entities::classes::BasePhysicalEntity *d, bool local, int floorlevel, int waterlevel, int material)
    {
        // This function seems to be used for playing material audio. No worries about that atm.
/*        if     (waterlevel>0) { if(material!=MAT_LAVA) playsound(S_SPLASHOUT, d==player1 ? NULL : &d->o); }
        else if(waterlevel<0) playsound(material==MAT_LAVA ? S_BURN : S_SPLASHIN, d==player1 ? NULL : &d->o);
        if     (floorlevel>0) { if(d==player1 || d->type!=ENT_PLAYER || ((gameent *)d)->ai) msgsound(S_JUMP, d); }
        else if(floorlevel<0) { if(d==player1 || d->type!=ENT_PLAYER || ((gameent *)d)->ai) msgsound(S_LAND, d); }*/
    }

    void ClearClients(bool notify)
    {
        loopv(game::server::clients) 
            if(game::server::clients[i]) 
                ClientDIsConnected(i, notify);
    }

    void InitClient() {
        // Setup the map time.
        maptime = 0;
		SpawnPlayer();
        findplayerspawn(game::player1);
    }

    const char *GameIdent() {
        return "SchizoMania";
    }
    const char *getscreenshotinfo() {
        return NULL;
    }


// >>>>>>>>>> SCRIPTBIND >>>>>>>>>>>>>> //
#if 0
#include "/Users/micha/dev/ScMaMike/src/build/binding/..+game+game.binding.cpp"
#endif
// <<<<<<<<<< SCRIPTBIND <<<<<<<<<<<<<< //

}; // namespace game.

