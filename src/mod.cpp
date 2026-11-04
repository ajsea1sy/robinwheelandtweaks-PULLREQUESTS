#include "mods/service.hpp"
#include "mods/svc/config.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.h"
#include "mods/svc/resource.h"
#include "mods/svc/ui.h"

#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "JSystem/J3DGraphBase/J3DPacket.h"
#include "JSystem/J3DGraphBase/J3DShapeDraw.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphBase/J3DTexture.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JUtility/JUTNameTab.h"
#include "d/actor/d_a_alink.h"
#include "d/d_a_item_static.h"
#include "d/actor/d_a_npc_tk.h"
#include "SSystem/SComponent/c_cc_d.h"
#include "d/d_com_inf_game.h"
#include "d/d_lib.h"
#include "d/d_meter2.h"
#include "d/d_particle_name.h"
#include "d/d_meter2_draw.h"
#include "d/d_menu_ring.h"
#include "d/d_menu_window.h"
#include "d/d_meter_map.h"
#include "d/d_meter2_info.h"
#include "d/d_meter_HIO.h"
#include "d/d_pane_class.h"
#include "d/d_select_cursor.h"
#include "Z2AudioLib/Z2AudioMgr.h"
#include "Z2AudioLib/Z2SceneMgr.h"
#include "Z2AudioLib/Z2SeqMgr.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_ext.h"

#include <dolphin/os.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

DEFINE_MOD();
IMPORT_SERVICE(ConfigService, svc_config);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(ResourceService, svc_resource);
IMPORT_SERVICE(UiService, svc_ui);

namespace {

constexpr int kWheelOpensLeft = 0;
constexpr int kWheelOpensDown = 1;
constexpr u8 kSitSlot = 0xFD;
constexpr u8 kGoronTunicSlot = 0xFE;
constexpr u8 kSitItemId = 0xF9;
constexpr u8 kGoronTunicItemId = 0xFB;
constexpr u8 kHawkGrassItem = 0xFD;
constexpr u8 kHoverBootsItem = 0xFC;
constexpr u8 kDarkLinkItem = 0xFA;
constexpr u8 kSecondarySlotBase = 0x80;
constexpr int kHawkGrassIconTexture = 0x59;
constexpr u8 kSecondaryWheelSlotCount = MAX_ITEM_SLOTS + 2;
constexpr f32 kSecondaryIconScale = 0.55f;
constexpr f32 kSecondaryCursorScale = 0.5f;
constexpr u32 kHoverLandingRecoveryFrames = 30;
constexpr u32 kGoronLavaReturnDelayFrames = 5 * 30;
constexpr size_t kItemTextureBufferSize = 0xC00;
constexpr char kHoverBootsIconResource[] = "hover_boots_wings.bti";
constexpr char kDarkLinkIconResource[] = "dark_link.bti";
constexpr char kSitIconResource[] = "sit.bti";
constexpr u8 kGoronTunicDisplayItem = dItemNo_WEAR_KOKIRI_e;
constexpr u32 kHawkAttackType =
    AT_TYPE_THROW_OBJ | AT_TYPE_NORMAL_SWORD | AT_TYPE_BOMB;
constexpr unsigned int kHawkIdleDespawnDelayFrames = 5 * 30;
constexpr unsigned int kHoverWaterMissToleranceFrames = 8;
constexpr u32 kDarkLinkTransitionFrames = 30;
constexpr f32 kSitModelGroundOffset = -28.0f;
constexpr f32 kSitModelForwardOffset = 65.0f;
constexpr int64_t kRupeeMagnetDefaultRadiusPercent = 100;
constexpr int64_t kRupeeMagnetMinRadiusPercent = 50;
constexpr int64_t kRupeeMagnetMaxRadiusPercent = 300;
constexpr f32 kRupeeMagnetBaseRadius = 300.0f;
constexpr u32 kFirstSitMusicBgmId = Z2BGM_FIELD_LINK_DAY;
constexpr size_t kSitMusicTrackCount = Z2BGM_TEMP_O - Z2BGM_FIELD_LINK_DAY + 1;
constexpr size_t kSitMusicOptionCount = kSitMusicTrackCount + 1;
static_assert(Z2BGM_TEMP_O == Z2BGM_FIELD_LINK_DAY + 0xAB);

constexpr const char* kSitMusicTrackNames[] = {
    "FIELD_LINK_DAY",
    "FIELD_LINK_NIGHT",
    "FIELD",
    "DUNGEON",
    "HORSE_BATTLE",
    "TOAL_VILLEGE",
    "COWBOY_GAME",
    "FACE_OFF_BATTLE",
    "BOOMERAMG_MONKEY",
    "DUNGEON_FOREST",
    "ITEM_GET",
    "ITEM_GET_MINI",
    "BOSSBABA_0",
    "BOSSBABA_1",
    "BOSSBABA_2",
    "BATTLE_NORMAL",
    "RANCH",
    "WILD_GOAT",
    "OPEN_BOX",
    "GAME_OVER",
    "ITEM_GET_ME",
    "HORSE_BATTLE_D02",
    "TWILIGHT",
    "EVENT01",
    "DUNGEON_LV2",
    "TOAL_NIGHT",
    "DEATH_MOUNTAIN01",
    "BATTLE_TWILIGHT",
    "HEART_GET",
    "MAGNE_GORON",
    "SHOP01",
    "SHOP02",
    "DUNGEON_LV3",
    "MAGNE_GORON_D01",
    "MAGNE_GORON_D02",
    "FILONE_FOREST",
    "SUMO",
    "BOSSFIREMAN_0",
    "KAKARIKO",
    "DEKUTOAD",
    "DEKUTOAD_D01",
    "RODEO",
    "BOSSFIREMAN_1",
    "SUMO_D1",
    "INDOOR",
    "DUNGEON_LV4",
    "STATUE_GAME",
    "EVENT02",
    "BOSS_OCTAEEL_0",
    "BOSS_OCTAEEL_1",
    "BOSS_OCTAEEL_D01",
    "BOSS_OCTAEEL_D02",
    "DESERT",
    "ZORA_VILLAGE",
    "BRIDGE_ON_FIRE",
    "DUNGEON_LV5",
    "DEMO10",
    "VARIANT",
    "CARGO_GAME_TW",
    "CASTLE_TOWN",
    "DUNGEON_LV6",
    "DUNGEON_LV7",
    "DUNGEON_LV8",
    "DUNGEON_LV9_02",
    "DEATH_MOUNTAIN02",
    "HOLY_FOREST",
    "FISHING",
    "FISHING_BARE",
    "FISHING_GET1",
    "FISHING_GET2",
    "FISHING_GET3",
    "LUTERA1",
    "LUTERA2",
    "FISHING_HIT",
    "SNOW_MOUNTAIN",
    "SNOW_BOARD",
    "BOSS_SNOWWOMAN_0",
    "BOSS_SNOWWOMAN_1",
    "DEMO08",
    "DEMO13_SEQ",
    "CHURCH1",
    "CHURCH2",
    "HOWL_TOBIKUSA",
    "HOWL_UMAKUSA",
    "HOWL_ZELDASONG",
    "HOWL_LIGHT_PRLD",
    "HIDDEN_VIL_D1",
    "HIDDEN_VILLAGE",
    "SHOP_CELEB",
    "SHOP_MARO",
    "LAKE",
    "STONE_ZELDASONG",
    "STONE_LIGHT_PRLD",
    "BOSS_SNOWWOMAN_D1",
    "ROOM_2",
    "MIDNA_SOS",
    "FORTUNE",
    "IB_MBOSS",
    "BOSS_ZANT",
    "IB_MBOSS_D01",
    "SUMOMO",
    "RAKKA_HOUSE",
    "LIGHT_PRLD_DUO",
    "SOUL_REQ_STONE",
    "SOUL_REQ_HOWL",
    "SOUL_REQ_DUO",
    "LUTERA_DEMO",
    "TEMPLE_OF_TIME",
    "TN_MBOSS",
    "OBACHAN",
    "RIVER_GAME",
    "GG_MBOSS",
    "OUGI_TRAINING",
    "HEALING_STONE",
    "HEALING_HOWL",
    "HEALING_DUO",
    "NEW_01_STONE",
    "NEW_01_HOWL",
    "NEW_01_DUO",
    "SUB_DUNGEON",
    "WCS_GAME",
    "WCS_D01",
    "MINIGAME_ROOM",
    "NEW_02_STONE",
    "NEW_02_HOWL",
    "NEW_02_DUO",
    "NEW_03_STONE",
    "NEW_03_HOWL",
    "NEW_03_DUO",
    "ITEM_GET_INSECT",
    "ITEM_GET_SMELL",
    "ITEM_GET_POU",
    "GG_MBOSS_D01",
    "P_ZANT",
    "VS_GANON_01",
    "VS_GANON_02",
    "VS_GANON_04",
    "HARAGIGANT_D01",
    "HARAGIGANT_D02",
    "HARAGIGANT_BTL01",
    "HARAGIGANT_BTL02",
    "DRAGON_D01",
    "DRAGON_D02",
    "DRAGON_BTL01",
    "DRAGON_BTL02",
    "KOROKORO_GAME",
    "YAMIMUSHI_B_D01",
    "GOMA_D01",
    "GOMA_BTL01",
    "GOMA_BTL02",
    "FACE_OFF_BATTLE2",
    "FACE_OFF_BATTLE3",
    "SNOWBOARD_WIN",
    "SNOWBOARD_LOSE",
    "INDOOR_03",
    "INDOOR_04",
    "MINIGAME_WIN01",
    "MINIGAME_WIN02",
    "POSTMAN",
    "EVENT05",
    "ITEM_GET_ME_S",
    "D30_01_NEXT_REQ",
    "D30_02_NEXT_REQ",
    "TARO_RESCUE",
    "KOMONJO_GET_INTRO",
    "RIVER_GAME_00",
    "ZORA_D01",
    "SUMO_D02",
    "TN_MBOSS_LV9",
    "MENU_SELECT",
    "TEMP",
    "TEMP_O",
};
static_assert(sizeof(kSitMusicTrackNames) / sizeof(kSitMusicTrackNames[0]) ==
              kSitMusicTrackCount);

struct SitMusicWaveMapping {
    u32 track;
    u8 wave1;
    u8 alternateWave1;
    u8 wave2 = 0;
};

constexpr SitMusicWaveMapping kSitMusicWaveMappings[] = {
    {Z2BGM_CARGO_GAME_TW, 0x24, 0},
    {Z2BGM_CASTLE_TOWN, 0x25, 0},
    {Z2BGM_CHURCH1, 0x2F, 0},
    {Z2BGM_CHURCH2, 0x30, 0},
    {Z2BGM_DEATH_MOUNTAIN01, 0x11, 0},
    {Z2BGM_DEATH_MOUNTAIN02, 0x11, 0},
    {Z2BGM_DEMO08, 0x21, 0},
    {Z2BGM_DEMO10, 0x21, 0},
    {Z2BGM_DESERT, 0x1F, 0},
    {Z2BGM_DUNGEON_FOREST, 0x0A, 0},
    {Z2BGM_DUNGEON_LV2, 0x13, 0},
    {Z2BGM_DUNGEON_LV3, 0x15, 0},
    {Z2BGM_DUNGEON_LV4, 0x1A, 0},
    {Z2BGM_DUNGEON_LV5, 0x1D, 0},
    {Z2BGM_DUNGEON_LV6, 0x26, 0},
    {Z2BGM_DUNGEON_LV7, 0x27, 0},
    {Z2BGM_DUNGEON_LV8, 0x28, 0},
    {Z2BGM_DUNGEON_LV9_02, 0x29, 0},
    {Z2BGM_EVENT01, 0x05, 0},
    {Z2BGM_EVENT02, 0x1C, 0},
    {Z2BGM_EVENT05, 0x55, 0},
    {Z2BGM_FIELD_LINK_DAY, 0x19, 0},
    {Z2BGM_FIELD_LINK_NIGHT, 0x19, 0},
    {Z2BGM_FILONE_FOREST, 0x0F, 0},
    {Z2BGM_FISHING, 0x2B, 0},
    {Z2BGM_FORTUNE, 0x37, 0},
    {Z2BGM_HIDDEN_VILLAGE, 0x31, 0},
    {Z2BGM_HOLY_FOREST, 0x2A, 0},
    {Z2BGM_HORSE_BATTLE, 0x0D, 0},
    {Z2BGM_INDOOR, 0x08, 0},
    {Z2BGM_INDOOR_03, 0x08, 0},
    {Z2BGM_INDOOR_04, 0x56, 0},
    {Z2BGM_KAKARIKO, 0x10, 0},
    {Z2BGM_LAKE, 0x34, 0},
    {Z2BGM_LUTERA2, 0x2C, 0},
    {Z2BGM_MIDNA_SOS, 0x36, 0},
    {Z2BGM_MINIGAME_ROOM, 0x47, 0x46},
    {Z2BGM_OBACHAN, 0x40, 0},
    {Z2BGM_RANCH, 0x01, 0},
    {Z2BGM_ROOM_2, 0x35, 0},
    {Z2BGM_SHOP01, 0x07, 0},
    {Z2BGM_SHOP02, 0x07, 0},
    {Z2BGM_SHOP_CELEB, 0x32, 0},
    {Z2BGM_SHOP_MARO, 0x33, 0},
    {Z2BGM_SNOW_MOUNTAIN, 0x2D, 0},
    {Z2BGM_SUB_DUNGEON, 0x45, 0},
    {Z2BGM_TEMPLE_OF_TIME, 0x3E, 0, 0x1B},
    {Z2BGM_TOAL_NIGHT, 0x06, 0},
    {Z2BGM_TOAL_VILLEGE, 0x03, 0},
    {Z2BGM_TWILIGHT, 0x0E, 0},
    {Z2BGM_ZORA_VILLAGE, 0x20, 0},
};

enum class SecondaryIconContext : u8 {
    None,
    Sit,
    GoronTunic,
    HawkGrass,
    HoverBoots,
    DarkLink,
};

enum class SitActionState : u8 {
    Inactive,
    Seated,
};

ConfigVarHandle g_wheelOpenDirection = 0;
ConfigVarHandle g_infiniteLavaWalk = 0;
ConfigVarHandle g_twilightWard = 0;
ConfigVarHandle g_hawkOneShotDamage = 0;
ConfigVarHandle g_hoverBootsWaterWalking = 0;
ConfigVarHandle g_rupeeMagnetEnabled = 0;
ConfigVarHandle g_rupeeMagnetRadiusPercent = 0;
ConfigVarHandle g_sitMusicTrack = 0;
std::array<std::array<char, 48>, kSitMusicOptionCount> g_sitMusicOptionLabelStorage{};
std::array<const char*, kSitMusicOptionCount> g_sitMusicOptionLabels{};
bool g_sitMusicOptionsInitialized = false;
bool g_sitMusicOverrideActive = false;
u32 g_sitMusicActiveTrack = 0;
u32 g_sitMusicReportedFailureTrack = 0;
u32 g_sitMusicWaveFailureTrack = 0;
std::array<bool, 256> g_sitMusicOwnedWaves{};
std::array<bool, 256> g_sitMusicWaveEraseFailureReported{};
std::array<bool, 256> g_sitMusicWaveReleaseScheduled{};
std::array<u8, 256> g_sitMusicWaveReleaseFrames{};
bool g_sitMusicModifiersSaved = false;
bool g_sitMusicPreviousHeightVolMod = false;
bool g_sitMusicPreviousTimeProcVolMod = false;
std::array<ConfigVarHandle, 2> g_secondaryAssignmentConfig{};
UiMenuTabHandle g_robinMenuTab = 0;
UiWindowHandle g_robinSettingsWindow = 0;
dMenu_Ring_c* g_secondaryRing = nullptr;
bool g_openingSecondaryRing = false;
bool g_goronTunicEquipped = false;
u32 g_goronLavaContactFrames = 0;
bool g_hoverBootsEquipped = false;
bool g_darkLinkFormActive = false;
SitActionState g_sitActionState = SitActionState::Inactive;
s16 g_sitOriginalShapeAngleX = 0;
s16 g_sitOriginalShapeAngleZ = 0;
f32 g_darkLinkFormBlend = 0.0f;
J3DModelData* g_darkLinkDrawModelData = nullptr;
// Shape packets render after modelDraw returns, so retain their model identities.
std::array<J3DModelData*, 10> g_darkLinkAppearanceModelData{};
u8 g_darkLinkAppearanceModelDataCount = 0;
struct DarkLinkDrawMaterialSnapshot {
    J3DMaterial* material = nullptr;
    u8 stageIndex = 0;
    u8 tevKColorSel = 0;
    bool isPupilMaterial = false;
    bool hasTevColor = false;
    bool hasTevKColor = false;
    f32 blend = 0.0f;
    J3DTevStage stage{};
    J3DGXColorS10 tevColor{};
    J3DGXColor tevKColor{};
};
std::array<DarkLinkDrawMaterialSnapshot, 64> g_darkLinkDrawMaterialSnapshots{};
u16 g_darkLinkDrawMaterialSnapshotCount = 0;
bool g_secondaryItemInputHandled = false;
bool g_hoverGroundHeightValid = false;
bool g_hoverLandingSequenceActive = false;
bool g_hoverAtSurface = false;
bool g_hoverWaterGroundPlaneActive = false;
f32 g_hoverWaterSurfaceHeight = 0.0f;
f32 g_hoverWaterUnderlyingGroundHeight = -G_CM3D_F_INF;
unsigned int g_hoverWaterMissFrames = 0;
bool g_secondaryHudAssignmentsInitialized = false;
SecondaryIconContext g_secondaryIconContext = SecondaryIconContext::None;
dMeter2_c* g_secondaryHudOwner = nullptr;
std::array<u8, MAX_SELECT_ITEM> g_secondaryHudAssignments{};
std::array<bool, MAX_SELECT_ITEM> g_secondaryHudRefreshPending{};
f32 g_hoverGroundHeight = 0.0f;
u32 g_hoverLandingFrames = 0;
u32 g_hoverRippleEmitter = 0;
bool g_hoverMagicEffectActive = false;
std::array<std::array<u32, 3>, 2> g_hoverMagicEmitters{};
bool g_primaryRingOpen = false;
bool g_pendingPrimaryItemSync = false;
bool g_deletingSecondaryRing = false;
bool g_hoverBootsTransitionPending = false;
int g_keyWaitDepth = 0;
int g_mapInputDepth = 0;
request_of_phase_process_class g_hawkGrassPhase{};
daAlink_c* g_pendingHawkGrassUse = nullptr;
u8 g_pendingHawkGrassButton = 0;
bool g_hawkGrassPhaseRequested = false;
bool g_hawkGrassArchiveLoaded = false;
fpc_ProcID g_spawnedHawkId = fpcM_ERROR_PROCESS_ID_e;
bool g_spawnedHawkAttackSeen = false;
bool g_spawnedHawkReturnSeen = false;
unsigned int g_spawnedHawkIdleFrames = 0;

std::array<u8, MAX_ITEM_SLOTS> g_secondarySlots = [] {
    std::array<u8, MAX_ITEM_SLOTS> slots{};
    slots.fill(dItemNo_NONE_e);
    slots[0] = kHawkGrassItem;
    slots[1] = kHoverBootsItem;
    slots[2] = kDarkLinkItem;
    return slots;
}();
std::array<u8, MAX_SELECT_ITEM> g_secondarySelectedItems = [] {
    std::array<u8, MAX_SELECT_ITEM> items{};
    items.fill(dItemNo_NONE_e);
    return items;
}();
std::array<u8, kSecondaryWheelSlotCount> g_secondaryWheelSlotIds{};
std::array<f32, kSecondaryWheelSlotCount> g_secondaryWheelSlotPosX{};
std::array<f32, kSecondaryWheelSlotCount> g_secondaryWheelSlotPosY{};
std::array<s16, kSecondaryWheelSlotCount> g_secondaryWheelSlotAngles{};
std::array<J2DPicture*, 3> g_tunicWheelItemTex{};
std::array<ResTIMG*, 3> g_tunicWheelItemBuf{};
JKRExpHeap* g_tunicWheelItemHeap = nullptr;
J2DPicture* g_sitWheelItemTex = nullptr;
ResTIMG* g_sitWheelItemBuf = nullptr;
JKRExpHeap* g_sitWheelItemHeap = nullptr;

bool wheelOpensDown() {
    int64_t value = kWheelOpensDown;
    return g_wheelOpenDirection != 0 &&
           svc_config->get_int(mod_ctx, g_wheelOpenDirection, &value) == MOD_OK &&
           value == kWheelOpensDown;
}

bool mapOpensDown() {
    return !wheelOpensDown();
}

bool get_bool_option(ConfigVarHandle handle, bool fallback) {
    bool value = fallback;
    if (handle == 0 || svc_config->get_bool(mod_ctx, handle, &value) != MOD_OK) {
        return fallback;
    }
    return value;
}

bool infiniteLavaWalkEnabled() {
    return get_bool_option(g_infiniteLavaWalk, false);
}

bool twilightWardEnabled() {
    return get_bool_option(g_twilightWard, false);
}

bool hawkOneShotDamageEnabled() {
    return get_bool_option(g_hawkOneShotDamage, false);
}

bool hoverBootsWaterWalkingEnabled() {
    return get_bool_option(g_hoverBootsWaterWalking, false);
}

bool hoverBootsAirwalkingEnabled() {
    return !hoverBootsWaterWalkingEnabled();
}

bool is_hover_boots_hovering(const daAlink_c* alink) {
    return alink != nullptr && hoverBootsAirwalkingEnabled() && g_hoverBootsEquipped &&
           g_hoverGroundHeightValid && g_hoverAtSurface &&
           !alink->mLinkAcch.ChkGroundHit() && !alink->checkEventRun() &&
           !alink->checkModeFlg(daAlink_c::MODE_SWIMMING);
}

bool rupeeMagnetEnabled() {
    return get_bool_option(g_rupeeMagnetEnabled, false);
}

int64_t rupeeMagnetRadiusPercent() {
    int64_t value = kRupeeMagnetDefaultRadiusPercent;
    if (g_rupeeMagnetRadiusPercent == 0 ||
        svc_config->get_int(mod_ctx, g_rupeeMagnetRadiusPercent, &value) != MOD_OK)
    {
        return kRupeeMagnetDefaultRadiusPercent;
    }
    return std::clamp(value, kRupeeMagnetMinRadiusPercent, kRupeeMagnetMaxRadiusPercent);
}

void initialize_sit_music_options() {
    if (g_sitMusicOptionsInitialized) {
        return;
    }

    constexpr char kDefaultLabel[] = "Default (Area Music)";
    std::memcpy(g_sitMusicOptionLabelStorage[0].data(), kDefaultLabel, sizeof(kDefaultLabel));
    g_sitMusicOptionLabels[0] = g_sitMusicOptionLabelStorage[0].data();
    for (size_t track = 0; track < kSitMusicTrackCount; ++track) {
        const char* source = std::strcmp(kSitMusicTrackNames[track], "EVENT01") == 0
            ? "Our Children Taken"
            : kSitMusicTrackNames[track];
        auto& label = g_sitMusicOptionLabelStorage[track + 1];
        bool capitalizeNext = true;
        size_t output = 0;
        for (size_t input = 0; source[input] != '\0' && output + 1 < label.size(); ++input) {
            const char character = source[input];
            if (character == '_') {
                label[output++] = ' ';
                capitalizeNext = true;
            } else {
                label[output++] = capitalizeNext
                    ? static_cast<char>(std::toupper(static_cast<unsigned char>(character)))
                    : static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
                capitalizeNext = false;
            }
        }
        label[output] = '\0';
        g_sitMusicOptionLabels[track + 1] = label.data();
    }
    g_sitMusicOptionsInitialized = true;
}

int64_t sitMusicSelection() {
    int64_t value = 0;
    if (g_sitMusicTrack == 0 ||
        svc_config->get_int(mod_ctx, g_sitMusicTrack, &value) != MOD_OK ||
        value < 0 || value >= static_cast<int64_t>(kSitMusicOptionCount))
    {
        return 0;
    }
    return value;
}

bool is_rupee(u8 itemNo) {
    switch (itemNo) {
    case dItemNo_GREEN_RUPEE_e:
    case dItemNo_BLUE_RUPEE_e:
    case dItemNo_YELLOW_RUPEE_e:
    case dItemNo_RED_RUPEE_e:
    case dItemNo_PURPLE_RUPEE_e:
    case dItemNo_ORANGE_RUPEE_e:
    case dItemNo_SILVER_RUPEE_e:
        return true;
    default:
        return false;
    }
}

bool isSecondaryRing(const dMenu_Ring_c* ring) {
    return ring != nullptr && ring == g_secondaryRing;
}

bool secondaryTunicAssigned(int button) {
    if (button < 0 || button > 1) {
        return false;
    }

    return g_secondarySelectedItems[button] == kGoronTunicSlot;
}

u8 secondaryItemIdForSlot(u8 slotId) {
    if (slotId == kSitSlot) {
        return kSitItemId;
    }
    if (slotId == kGoronTunicSlot) {
        return kGoronTunicItemId;
    }

    if (slotId >= kSecondarySlotBase &&
        slotId < kSecondarySlotBase + MAX_ITEM_SLOTS)
    {
        return g_secondarySlots[slotId - kSecondarySlotBase];
    }

    return dItemNo_NONE_e;
}

u8 secondaryDisplayItemForId(u8 itemId) {
    if (itemId == kSitItemId) {
        return dItemNo_WOOD_STICK_e;
    }
    if (itemId == kGoronTunicItemId) {
        return kGoronTunicDisplayItem;
    }
    if (itemId == kDarkLinkItem) {
        return dItemNo_WOOD_STICK_e;
    }
    if (itemId == kHawkGrassItem || itemId == kHoverBootsItem) {
        return dItemNo_WOOD_STICK_e;
    }
    return itemId;
}

u8 secondaryDisplayItemForSlot(u8 slotId) {
    return secondaryDisplayItemForId(secondaryItemIdForSlot(slotId));
}

bool isHawkGrassSlot(u8 slotId) {
    return slotId >= kSecondarySlotBase &&
           slotId < kSecondarySlotBase + MAX_ITEM_SLOTS &&
           g_secondarySlots[slotId - kSecondarySlotBase] == kHawkGrassItem;
}

SecondaryIconContext secondaryIconContextForSlot(u8 slotId) {
    if (slotId == kSitSlot) {
        return SecondaryIconContext::Sit;
    }
    if (slotId == kGoronTunicSlot) {
        return SecondaryIconContext::GoronTunic;
    }
    if (isHawkGrassSlot(slotId)) {
        return SecondaryIconContext::HawkGrass;
    }
    if (slotId >= kSecondarySlotBase &&
        slotId < kSecondarySlotBase + MAX_ITEM_SLOTS &&
        g_secondarySlots[slotId - kSecondarySlotBase] == kDarkLinkItem)
    {
        return SecondaryIconContext::DarkLink;
    }
    if (slotId >= kSecondarySlotBase &&
        slotId < kSecondarySlotBase + MAX_ITEM_SLOTS &&
        g_secondarySlots[slotId - kSecondarySlotBase] == kHoverBootsItem)
    {
        return SecondaryIconContext::HoverBoots;
    }
    return SecondaryIconContext::None;
}

int begin_hawk_grass_whistle(daAlink_c* alink, u8 button) {
    if (alink == nullptr || alink->mProcID == daAlink_c::PROC_GRASS_WHISTLE_WAIT) {
        return 1;
    }

    if (dComIfG_getObjectRes("J_Tobi", 3) == nullptr) {
        svc_log->error(mod_ctx, "Hawk Grass model archive loaded without its model resource");
        return 0;
    }

    alink->mSelectItemId = button;
    const int result = alink->procGrassWhistleWaitInit(0, -1, 0, 0, nullptr);
    if (alink->mProcID != daAlink_c::PROC_GRASS_WHISTLE_WAIT) {
        svc_log->warn(mod_ctx, "Hawk Grass whistle action was rejected by the game");
        return result;
    }

    alink->mLeftHandIndex = 0xFE;
    alink->setGrassWhistleModel(0);
    svc_log->info(mod_ctx, "Hawk Grass equipped; press A to play or B to cancel");
    return result;
}

void update_hawk_grass_resource() {
    if (!g_hawkGrassPhaseRequested || g_hawkGrassArchiveLoaded) {
        return;
    }

    const cPhs_Step phase = dComIfG_resLoad(&g_hawkGrassPhase, "J_Tobi");
    if (phase == cPhs_COMPLEATE_e) {
        g_hawkGrassArchiveLoaded = true;
        svc_log->info(mod_ctx, "Hawk Grass model archive loaded");
        if (g_pendingHawkGrassUse != nullptr) {
            auto* alink = g_pendingHawkGrassUse;
            const u8 button = g_pendingHawkGrassButton;
            g_pendingHawkGrassUse = nullptr;
            begin_hawk_grass_whistle(alink, button);
        }
    } else if (phase == cPhs_ERROR_e || phase == cPhs_UNK3_e) {
        g_pendingHawkGrassUse = nullptr;
        g_hawkGrassPhaseRequested = false;
        svc_log->error(mod_ctx, "Failed to load the Hawk Grass model archive");
    }
}

const char* secondaryItemNameForSlot(u8 slotId) {
    if (slotId == kSitSlot) {
        return "Sit";
    }
    if (slotId == kGoronTunicSlot) {
        return "Goron Tunic";
    }

    if (slotId >= kSecondarySlotBase &&
        slotId < kSecondarySlotBase + MAX_ITEM_SLOTS)
    {
        const u8 item = g_secondarySlots[slotId - kSecondarySlotBase];
        if (item == kHawkGrassItem) {
            return "Hawk Grass";
        }
        if (item == kHoverBootsItem) {
            return "Hover Boots";
        }
        if (item == kDarkLinkItem) {
            return "Dark Link Form";
        }
    }

    return "";
}

bool secondaryHawkGrassAssigned(int button) {
    if (button < 0 || button > 1) {
        return false;
    }

    const u8 slotId = g_secondarySelectedItems[button];
    return slotId >= kSecondarySlotBase &&
           slotId < kSecondarySlotBase + MAX_ITEM_SLOTS &&
           g_secondarySlots[slotId - kSecondarySlotBase] == kHawkGrassItem;
}

bool secondaryHoverBootsAssigned(int button) {
    if (button < 0 || button > 1) {
        return false;
    }

    const u8 slotId = g_secondarySelectedItems[button];
    return slotId >= kSecondarySlotBase &&
           slotId < kSecondarySlotBase + MAX_ITEM_SLOTS &&
           g_secondarySlots[slotId - kSecondarySlotBase] == kHoverBootsItem;
}

void spawn_hawk_for_grass(daAlink_c* alink) {
    auto* existingHawk = static_cast<daNPC_TK_c*>(fopAcM_SearchByName(fpcNm_NPC_TK_e));
    if (existingHawk != nullptr) {
        g_spawnedHawkId = fopAcM_GetID(existingHawk);
        g_spawnedHawkAttackSeen = false;
        g_spawnedHawkReturnSeen = false;
        g_spawnedHawkIdleFrames = 0;
        svc_log->info(mod_ctx, "Tracking Hawk selected by modded Hawk Grass");
        return;
    }

    cXyz spawnPosition = alink->current.pos;
    spawnPosition.y += 300.0f;
    const fpc_ProcID hawkId = fopAcM_create(
        fpcNm_NPC_TK_e, 0xFFFFFFFF, &spawnPosition, fopAcM_GetRoomNo(alink),
        &alink->shape_angle, nullptr, -1);
    if (hawkId == fpcM_ERROR_PROCESS_ID_e) {
        svc_log->error(mod_ctx, "Failed to create Hawk actor for Hawk Grass");
    } else {
        g_spawnedHawkId = hawkId;
        g_spawnedHawkAttackSeen = false;
        g_spawnedHawkReturnSeen = false;
        g_spawnedHawkIdleFrames = 0;
        svc_log->info(mod_ctx, "Created Hawk actor for Hawk Grass");
    }
}

void spawn_hawk_after_grass_sound(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    const u32 soundId = mods::arg<u32>(args, 1);
    if (alink == nullptr || soundId != Z2SE_PLAY_REED_TOBIKUSA ||
        alink->mProcID != daAlink_c::PROC_GRASS_WHISTLE_WAIT ||
        alink->field_0x3198 != 0x100 || !secondaryHawkGrassAssigned(alink->mSelectItemId))
    {
        return;
    }

    spawn_hawk_for_grass(alink);
}

void update_spawned_hawk_cleanup() {
    if (g_spawnedHawkId == fpcM_ERROR_PROCESS_ID_e) {
        return;
    }

    fopAc_ac_c* actor = nullptr;
    if (!fopAcM_SearchByID(g_spawnedHawkId, &actor) || actor == nullptr) {
        g_spawnedHawkId = fpcM_ERROR_PROCESS_ID_e;
        g_spawnedHawkAttackSeen = false;
        g_spawnedHawkReturnSeen = false;
        g_spawnedHawkIdleFrames = 0;
        return;
    }

    auto* hawk = static_cast<daNPC_TK_c*>(actor);
    const bool hawkAttacking = hawk->mActionType == 3;
    hawk->mSphere.SetAtType(hawkAttacking ? kHawkAttackType : AT_TYPE_THROW_OBJ);
    hawk->mSphere.SetAtAtp(hawkAttacking && hawkOneShotDamageEnabled() ? 80 : 1);
    if (hawk->mActionType == 3) {
        g_spawnedHawkAttackSeen = true;
    }
    if (g_spawnedHawkAttackSeen && hawk->mActionType == 4) {
        g_spawnedHawkReturnSeen = true;
    }

    auto* alink = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    const bool linkUsingHawk =
        alink != nullptr &&
        (alink->mProcID == daAlink_c::PROC_HAWK_CATCH ||
            alink->mProcID == daAlink_c::PROC_HAWK_SUBJECT);
    if (g_spawnedHawkAttackSeen && g_spawnedHawkReturnSeen &&
        hawk->mActionType == 0 && !linkUsingHawk)
    {
        ++g_spawnedHawkIdleFrames;
    } else {
        g_spawnedHawkIdleFrames = 0;
    }

    if (g_spawnedHawkIdleFrames >= kHawkIdleDespawnDelayFrames) {
        fopAcM_delete(hawk);
        g_spawnedHawkId = fpcM_ERROR_PROCESS_ID_e;
        g_spawnedHawkAttackSeen = false;
        g_spawnedHawkReturnSeen = false;
        g_spawnedHawkIdleFrames = 0;
        svc_log->info(mod_ctx, "Removed modded Hawk after five idle seconds");
        return;
    }

}

bool secondaryItemAssigned(int button) {
    if (button < 0 || button > 1) {
        return false;
    }

    return secondaryItemIdForSlot(g_secondarySelectedItems[button]) != dItemNo_NONE_e;
}

bool isValidSecondaryAssignment(int64_t slotId) {
    return slotId == dItemNo_NONE_e || slotId == kSitSlot ||
           slotId == kGoronTunicSlot ||
           (slotId >= kSecondarySlotBase &&
            slotId < kSecondarySlotBase + MAX_ITEM_SLOTS);
}

ModResult load_secondary_assignments() {
    for (int button = 0; button < 2; ++button) {
        int64_t slotId = dItemNo_NONE_e;
        const ModResult result =
            svc_config->get_int(mod_ctx, g_secondaryAssignmentConfig[button], &slotId);
        if (result != MOD_OK) {
            svc_log->error(mod_ctx, "failed to load private secondary item assignment");
            return result;
        }
        if (!isValidSecondaryAssignment(slotId)) {
            svc_log->warn(mod_ctx, "discarded invalid private secondary item assignment");
            slotId = dItemNo_NONE_e;
            const ModResult saveResult = svc_config->set_int(
                mod_ctx, g_secondaryAssignmentConfig[button], slotId);
            if (saveResult != MOD_OK) {
                svc_log->error(mod_ctx, "failed to clear invalid secondary item assignment");
                return saveResult;
            }
        }
        g_secondarySelectedItems[button] = static_cast<u8>(slotId);
    }
    return MOD_OK;
}

void save_secondary_assignment(int button, u8 slotId) {
    if (button < 0 || button > 1 || g_secondaryAssignmentConfig[button] == 0) {
        return;
    }
    const ModResult result =
        svc_config->set_int(mod_ctx, g_secondaryAssignmentConfig[button], slotId);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to save private secondary item assignment");
    }
}

void clear_hover_water_ground_plane() {
    g_hoverWaterGroundPlaneActive = false;
    g_hoverWaterUnderlyingGroundHeight = -G_CM3D_F_INF;
    g_hoverWaterMissFrames = 0;
}

bool is_lava_surface_contact(const daAlink_c* alink) {
    return infiniteLavaWalkEnabled() && g_goronTunicEquipped && alink != nullptr &&
           alink->field_0x2fbc == 6 &&
           alink->mWaterY != -G_CM3D_F_INF && alink->mLinkAcch.ChkGroundHit() &&
           alink->current.pos.y <= alink->mWaterY + 1.0f &&
           alink->current.pos.y >= alink->mWaterY - alink->mHeight;
}

bool is_lava_polygon_contact(const daAlink_c* alink) {
    return alink != nullptr &&
           (alink->field_0x2fbc == 6 ||
            (alink->checkModeFlg(0x40) && alink->field_0x2fc5 == 6));
}

f32 get_lava_surface_y(const daAlink_c* alink) {
    if (alink->field_0x2fbc == 6 && alink->mWaterY != -G_CM3D_F_INF) {
        return alink->mWaterY;
    }
    if (alink->checkModeFlg(0x40) && alink->field_0x2fc5 == 6 &&
        alink->field_0x33b8 != -G_CM3D_F_INF)
    {
        return alink->field_0x33b8;
    }
    return -G_CM3D_F_INF;
}

bool is_goron_lava_surface_contact(const daAlink_c* alink) {
    if (alink == nullptr || !alink->mLinkAcch.ChkGroundHit() ||
        !is_lava_polygon_contact(alink))
    {
        return false;
    }

    const f32 surfaceY = get_lava_surface_y(alink);
    return surfaceY != -G_CM3D_F_INF &&
           alink->current.pos.y <= surfaceY + 1.0f &&
           alink->current.pos.y >= surfaceY - alink->mHeight;
}

bool stabilize_lava_surface_contact(daAlink_c* alink, bool resetSinkOffset) {
    if (!is_lava_polygon_contact(alink)) {
        return false;
    }

    const f32 surfaceY = get_lava_surface_y(alink);
    if (surfaceY == -G_CM3D_F_INF ||
        alink->current.pos.y > surfaceY + 1.0f ||
        alink->current.pos.y < surfaceY - alink->mHeight)
    {
        return false;
    }

    if (alink->speed.y > 0.0f && alink->current.pos.y > surfaceY) {
        return false;
    }

    alink->current.pos.y = surfaceY;
    alink->speed.y = 0.0f;
    alink->mLinkAcch.m_ground_h = surfaceY;
    alink->mLinkAcch.SetGroundHit();
    alink->mLinkAcch.SetGroundFind();
    alink->mLinkAcch.ClrGroundLanding();
    alink->mLinkAcch.ClrGroundAway();
    alink->offModeFlg(daAlink_c::MODE_SWIMMING);
    alink->mGndPolyAtt0 = 3;
    alink->mGndPolyAtt1 = 1;
    alink->mGroundCode = 3;
    if (resetSinkOffset) {
        alink->mSinkShapeOffset = 0.0f;
    }
    return true;
}

void update_goron_lava_contact_timer() {
    auto* alink = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    if (!g_goronTunicEquipped || infiniteLavaWalkEnabled() || alink == nullptr ||
        alink->checkEventRun() || !is_goron_lava_surface_contact(alink))
    {
        g_goronLavaContactFrames = 0;
        return;
    }

    if (g_goronLavaContactFrames < kGoronLavaReturnDelayFrames) {
        ++g_goronLavaContactFrames;
    }
}

void stop_hover_magic_effects() {
    if (!g_hoverMagicEffectActive) {
        return;
    }

    for (auto& footEmitters : g_hoverMagicEmitters) {
        for (u32& emitterId : footEmitters) {
            if (emitterId != 0) {
                dComIfGp_particle_setStopContinue(emitterId);
                emitterId = 0;
            }
        }
    }
    g_hoverMagicEffectActive = false;
}

void stop_hover_foot_effects(int foot) {
    for (u32& emitterId : g_hoverMagicEmitters[foot]) {
        if (emitterId != 0) {
            dComIfGp_particle_setStopContinue(emitterId);
            emitterId = 0;
        }
    }
}

void update_hover_magic_effect(daAlink_c* alink) {
    if (!g_hoverBootsEquipped || alink == nullptr || alink->mpLinkModel == nullptr) {
        stop_hover_magic_effects();
        return;
    }

    static constexpr u16 effectIds[] = {
        ID_ZI_J_HB_MAGNE_A,
        ID_ZI_J_HB_MAGNE_B,
        ID_ZI_J_HB_MAGNE_C,
    };
    g_hoverMagicEffectActive = true;

    for (int foot = 0; foot < 2; ++foot) {
        const cXyz& footPosition = foot == 0 ? alink->mLeftFootPos : alink->mRightFootPos;
        const cXyz localFootPosition = footPosition - alink->current.pos;
        if (localFootPosition.y >= 6.0f) {
            stop_hover_foot_effects(foot);
            continue;
        }

        const u16 footJoint = foot == 0 ? alink->field_0x30bc : alink->field_0x30be;
        MtxP footMatrix = alink->mpLinkModel->getAnmMtx(footJoint);
        if (footMatrix == nullptr) {
            stop_hover_foot_effects(foot);
            continue;
        }

        for (int effect = 0; effect < 3; ++effect) {
            u32& emitterId = g_hoverMagicEmitters[foot][effect];
            emitterId =
                dComIfGp_particle_set(emitterId, effectIds[effect], &alink->current.pos,
                                      &alink->tevStr);
            if (emitterId == 0) {
                continue;
            }
            dComIfGp_particle_levelEmitterOnEventMove(emitterId);
            JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(emitterId);
            if (emitter != nullptr) {
                emitter->setGlobalRTMatrix(footMatrix);
            }
        }
    }
}

void update_hover_boots() {
    auto* alink = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    if (hoverBootsWaterWalkingEnabled()) {
        g_hoverRippleEmitter = 0;
        return;
    }

    if (!g_hoverBootsEquipped || alink == nullptr || alink->checkEventRun() ||
        alink->checkModeFlg(daAlink_c::MODE_SWIMMING))
    {
        g_hoverRippleEmitter = 0;
        if (!g_hoverBootsEquipped || alink == nullptr) {
            g_hoverGroundHeightValid = false;
            g_hoverLandingSequenceActive = false;
            g_hoverAtSurface = false;
            g_hoverLandingFrames = 0;
        }
        return;
    }

    if (alink->mLinkAcch.ChkGroundHit()) {
        g_hoverRippleEmitter = 0;
        return;
    }

    if (g_hoverGroundHeightValid) {
        cXyz ripplePosition = alink->current.pos;
        ripplePosition.y = g_hoverGroundHeight + 2.0f;
        fopAcM_effHamonSet(&g_hoverRippleEmitter, &ripplePosition, 1.0f, 0.1f);
    }
}

void disable_hover_boots() {
    clear_hover_water_ground_plane();
    g_hoverBootsEquipped = false;
    g_hoverGroundHeightValid = false;
    g_hoverLandingSequenceActive = false;
    g_hoverAtSurface = false;
    g_hoverLandingFrames = 0;
    g_hoverRippleEmitter = 0;
    stop_hover_magic_effects();
}

void set_hover_boots_enabled(daAlink_c* alink, bool enabled) {
    if (enabled == g_hoverBootsEquipped) {
        return;
    }

    if (!enabled) {
        disable_hover_boots();
    } else {
        g_hoverBootsEquipped = true;
        g_hoverGroundHeight = alink->current.pos.y;
        g_hoverGroundHeightValid = true;
        g_hoverLandingSequenceActive = false;
        g_hoverAtSurface = false;
        g_hoverLandingFrames = 0;
        g_hoverRippleEmitter = 0;
    }
    svc_log->info(mod_ctx, g_hoverBootsEquipped ? "Hover Boots equipped" :
                                                 "Hover Boots unequipped");
}

void tint_tunic_icon(J2DPicture* icon) {
    if (icon != nullptr) {
        icon->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
            JUtility::TColor(190, 24, 24, 255));
    }
}

void tint_tunic_icon(CPaneMgr* icon) {
    if (icon != nullptr) {
        icon->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
            JUtility::TColor(190, 24, 24, 255));
    }
}

void tint_secondary_ring_icons(dMenu_Ring_c* ring) {
    if (!isSecondaryRing(ring)) {
        return;
    }

    for (auto* icon : g_tunicWheelItemTex) {
        tint_tunic_icon(icon);
    }

    for (int button = 0; button < 2; ++button) {
        if (!secondaryTunicAssigned(button)) {
            continue;
        }
        for (auto* icon : ring->mpSelectItemTex[button]) {
            tint_tunic_icon(icon);
        }
    }
}

void clear_tunic_wheel_textures() {
    for (auto*& picture : g_tunicWheelItemTex) {
        if (picture != nullptr) {
            JKR_DELETE(picture);
            picture = nullptr;
        }
    }
    if (g_tunicWheelItemHeap != nullptr) {
        for (auto*& buffer : g_tunicWheelItemBuf) {
            if (buffer != nullptr) {
                g_tunicWheelItemHeap->free(buffer);
                buffer = nullptr;
            }
        }
    }
    g_tunicWheelItemHeap = nullptr;
}

void clear_sit_wheel_texture() {
    if (g_sitWheelItemTex != nullptr) {
        JKR_DELETE(g_sitWheelItemTex);
        g_sitWheelItemTex = nullptr;
    }
    if (g_sitWheelItemHeap != nullptr && g_sitWheelItemBuf != nullptr) {
        g_sitWheelItemHeap->free(g_sitWheelItemBuf);
    }
    g_sitWheelItemBuf = nullptr;
    g_sitWheelItemHeap = nullptr;
}

void load_sit_wheel_texture(dMenu_Ring_c* ring) {
    clear_sit_wheel_texture();
    ResourceBuffer icon = RESOURCE_BUFFER_INIT;
    const ModResult result =
        svc_resource->load(mod_ctx, kSitIconResource, &icon);
    if (result != MOD_OK || icon.data == nullptr || icon.size == 0) {
        svc_log->error(mod_ctx, "failed to load the Sit wheel icon");
        svc_resource->free(mod_ctx, &icon);
        return;
    }
    if (icon.size < sizeof(ResTIMG) || icon.size > kItemTextureBufferSize) {
        svc_log->error(mod_ctx, "Sit wheel icon has an invalid texture size");
        svc_resource->free(mod_ctx, &icon);
        return;
    }

    g_sitWheelItemHeap = ring->mpHeap;
    g_sitWheelItemBuf =
        static_cast<ResTIMG*>(g_sitWheelItemHeap->alloc(kItemTextureBufferSize, 0x20));
    if (g_sitWheelItemBuf == nullptr) {
        g_sitWheelItemHeap = nullptr;
        svc_log->error(mod_ctx, "failed to allocate the Sit wheel icon buffer");
        svc_resource->free(mod_ctx, &icon);
        return;
    }
    std::memcpy(g_sitWheelItemBuf, icon.data, icon.size);
    svc_resource->free(mod_ctx, &icon);
    DCStoreRangeNoSync(g_sitWheelItemBuf, kItemTextureBufferSize);

    g_sitWheelItemTex = JKR_NEW J2DPicture(g_sitWheelItemBuf);
    if (g_sitWheelItemTex == nullptr) {
        g_sitWheelItemHeap->free(g_sitWheelItemBuf);
        g_sitWheelItemBuf = nullptr;
        g_sitWheelItemHeap = nullptr;
        svc_log->error(mod_ctx, "failed to create the Sit wheel icon picture");
        return;
    }
    g_sitWheelItemTex->setBasePosition(J2DBasePosition_4);
}

void load_tunic_wheel_textures(dMenu_Ring_c* ring) {
    clear_tunic_wheel_textures();
    g_tunicWheelItemHeap = ring->mpHeap;
    for (auto*& buffer : g_tunicWheelItemBuf) {
        buffer = static_cast<ResTIMG*>(ring->mpHeap->alloc(0xC00, 0x20));
    }

    const int textureCount = dMeter2Info_readItemTexture(
        kGoronTunicDisplayItem, g_tunicWheelItemBuf[0], nullptr, g_tunicWheelItemBuf[1], nullptr,
        g_tunicWheelItemBuf[2], nullptr, nullptr, nullptr, -1);
    for (int texture = 0; texture < std::min(textureCount, 3); ++texture) {
        g_tunicWheelItemTex[texture] = JKR_NEW J2DPicture(g_tunicWheelItemBuf[texture]);
        g_tunicWheelItemTex[texture]->setBasePosition(J2DBasePosition_4);
    }
    dMeter2Info_setItemColor(kGoronTunicDisplayItem, g_tunicWheelItemTex[0],
        g_tunicWheelItemTex[1], g_tunicWheelItemTex[2], nullptr);
    tint_secondary_ring_icons(ring);
}

void setup_secondary_ring(dMenu_Ring_c* ring) {
    g_secondaryWheelSlotIds.fill(dItemNo_NONE_e);
    for (u8 slot = 0; slot < MAX_ITEM_SLOTS; ++slot) {
        g_secondaryWheelSlotIds[slot] = kSecondarySlotBase + slot;
    }
    g_secondaryWheelSlotIds[MAX_ITEM_SLOTS] = kSitSlot;
    g_secondaryWheelSlotIds[MAX_ITEM_SLOTS + 1] = kGoronTunicSlot;

    ring->mItemsTotal = MAX_ITEM_SLOTS;
    ring->mTotalItemTexToAlloc = MAX_ITEM_SLOTS;
    ring->field_0x634 = 0x10000 / kSecondaryWheelSlotCount;
    ring->mCurrentSlot = 0;
    ring->mXButtonSlot = dItemNo_NONE_e;
    ring->mYButtonSlot = dItemNo_NONE_e;
    ring->mStatus = dMenu_Ring_c::STATUS_WAIT;
    ring->mOldStatus = dMenu_Ring_c::STATUS_WAIT;
    ring->field_0x674[0] = 0;
    ring->field_0x674[1] = 0;
    ring->field_0x674[2] = 0;
    ring->field_0x674[3] = 0;
    ring->mRingRadiusH = g_ringHIO.mRingRadiusH;
    ring->mRingRadiusV = g_ringHIO.mRingRadiusV;

    const f32 centerX = g_ringHIO.mItemRingPosX + FB_WIDTH_BASE / 2;
    const f32 centerY = g_ringHIO.mItemRingPosY + FB_HEIGHT_BASE / 2;
    constexpr f32 kTau = 6.28318530717958647692f;
    constexpr int kEllipseSamples = 4096;
    std::array<f32, kEllipseSamples + 1> arcLengths{};
    f32 previousX = 0.0f;
    f32 previousY = -ring->mRingRadiusV;
    for (int sample = 1; sample <= kEllipseSamples; ++sample) {
        const f32 angle = kTau * sample / kEllipseSamples;
        const f32 x = ring->mRingRadiusH * std::sin(angle);
        const f32 y = -ring->mRingRadiusV * std::cos(angle);
        const f32 deltaX = x - previousX;
        const f32 deltaY = y - previousY;
        arcLengths[sample] =
            arcLengths[sample - 1] + std::sqrt(deltaX * deltaX + deltaY * deltaY);
        previousX = x;
        previousY = y;
    }

    const f32 perimeter = arcLengths[kEllipseSamples];
    for (u8 index = 0; index < kSecondaryWheelSlotCount; ++index) {
        const f32 targetDistance = perimeter * index / kSecondaryWheelSlotCount;
        int sample = 1;
        while (sample < kEllipseSamples &&
            arcLengths[sample] < targetDistance)
        {
            ++sample;
        }
        const f32 segmentLength = arcLengths[sample] - arcLengths[sample - 1];
        const f32 segmentFraction = segmentLength > 0.0f
            ? (targetDistance - arcLengths[sample - 1]) / segmentLength
            : 0.0f;
        const f32 angle = kTau * (sample - 1 + segmentFraction) / kEllipseSamples;
        g_secondaryWheelSlotPosX[index] =
            centerX + ring->mRingRadiusH * std::sin(angle);
        g_secondaryWheelSlotPosY[index] =
            centerY - ring->mRingRadiusV * std::cos(angle);
        g_secondaryWheelSlotAngles[index] = cM_atan2s(
            g_secondaryWheelSlotPosX[index] - centerX,
            g_secondaryWheelSlotPosY[index] - centerY);
    }

    for (int i = 0; i < MAX_ITEM_SLOTS; ++i) {
        ring->mItemSlots[i] = dItemNo_NONE_e;
        ring->mItemSlotPosX[i] = g_secondaryWheelSlotPosX[i];
        ring->mItemSlotPosY[i] = g_secondaryWheelSlotPosY[i];
        ring->field_0x63e[i] = g_secondaryWheelSlotAngles[i];
        ring->mItemSlotParam1[i] = 0.0f;
        ring->mItemSlotParam2[i] = 0.0f;
        for (int j = 0; j < 3; ++j) {
            if (ring->mpItemTex[i][j] != nullptr) {
                JKR_DELETE(ring->mpItemTex[i][j]);
                ring->mpItemTex[i][j] = nullptr;
            }
            if (ring->mpItemBuf[i][j] != nullptr) {
                ring->mpHeap->free(ring->mpItemBuf[i][j]);
                ring->mpItemBuf[i][j] = nullptr;
            }
        }
    }

    for (u8 index = 0; index < MAX_ITEM_SLOTS; ++index) {
        const u8 item = secondaryItemIdForSlot(g_secondaryWheelSlotIds[index]);
        const u8 iconItem = secondaryDisplayItemForSlot(g_secondaryWheelSlotIds[index]);
        ring->mItemSlots[index] = g_secondaryWheelSlotIds[index];
        if (item == dItemNo_NONE_e) {
            continue;
        }
        for (int texture = 0; texture < 3; ++texture) {
            ring->mpItemBuf[index][texture] =
                static_cast<ResTIMG*>(ring->mpHeap->alloc(0xC00, 0x20));
        }

        const SecondaryIconContext previousIconContext = g_secondaryIconContext;
        g_secondaryIconContext =
            secondaryIconContextForSlot(g_secondaryWheelSlotIds[index]);
        const int textureCount = dMeter2Info_readItemTexture(
            iconItem, ring->mpItemBuf[index][0], nullptr, ring->mpItemBuf[index][1], nullptr,
            ring->mpItemBuf[index][2], nullptr, nullptr, nullptr,
            isHawkGrassSlot(g_secondaryWheelSlotIds[index]) ? kHawkGrassIconTexture : -1);
        g_secondaryIconContext = previousIconContext;
        for (int texture = 0; texture < std::min(textureCount, 3); ++texture) {
            ring->mpItemTex[index][texture] = JKR_NEW J2DPicture(ring->mpItemBuf[index][texture]);
            ring->mpItemTex[index][texture]->setBasePosition(J2DBasePosition_4);
        }
        dMeter2Info_setItemColor(iconItem, ring->mpItemTex[index][0], ring->mpItemTex[index][1],
            ring->mpItemTex[index][2], nullptr);
        const f32 scale = dItem_data::getTexScale(iconItem) / 100.0f;
        ring->mItemSlotParam1[index] =
            ring->mpItemBuf[index][0]->width / 48.0f * scale * kSecondaryIconScale;
        ring->mItemSlotParam2[index] =
            ring->mpItemBuf[index][0]->height / 48.0f * scale * kSecondaryIconScale;
    }
    load_sit_wheel_texture(ring);
    load_tunic_wheel_textures(ring);

    for (int button = 0; button < 2; ++button) {
        const u8 assignedSlot = g_secondarySelectedItems[button];
        ring->setSelectItem(button, secondaryDisplayItemForSlot(assignedSlot));
        if (button == 0) {
            ring->mXButtonSlot = dItemNo_NONE_e;
        } else {
            ring->mYButtonSlot = dItemNo_NONE_e;
        }
        for (u8 index = 0; index < kSecondaryWheelSlotCount; ++index) {
            if (g_secondaryWheelSlotIds[index] == assignedSlot) {
                if (button == 0) {
                    ring->mXButtonSlot = index;
                } else {
                    ring->mYButtonSlot = index;
                }
                break;
            }
        }
        if (secondaryTunicAssigned(button)) {
            for (auto* icon : ring->mpSelectItemTex[button]) {
                tint_tunic_icon(icon);
            }
        }
    }
    ring->mpDrawCursor->setPos(
        g_secondaryWheelSlotPosX[0], g_secondaryWheelSlotPosY[0]);
    ring->mpDrawCursor->setParam(
        kSecondaryCursorScale, kSecondaryCursorScale, 0.1f, 0.6f, 0.5f);
    tint_secondary_ring_icons(ring);
}

void set_secondary_ring_name(dMenu_Ring_c* ring) {
    if (ring->mpCenterScreen == nullptr || ring->mCurrentSlot >= kSecondaryWheelSlotCount) {
        return;
    }

    constexpr unsigned long long names[] = {
        MULTI_CHAR('item_n04'), MULTI_CHAR('item_n05'), MULTI_CHAR('item_n06'),
        MULTI_CHAR('item_n07'), MULTI_CHAR('fitem_n1'), MULTI_CHAR('fitem_n2'),
        MULTI_CHAR('fitem_n3'), MULTI_CHAR('fitem_n4')};
    for (const auto nameId : names) {
        if (auto* name = static_cast<J2DTextBox*>(ring->mpCenterScreen->search(nameId))) {
            name->setString(0x40,
                secondaryItemNameForSlot(g_secondaryWheelSlotIds[ring->mCurrentSlot]));
        }
    }
}

u8 closest_secondary_wheel_slot(s16 stickAngle) {
    u8 closestSlot = 0;
    int smallestDifference = 0x10000;
    for (u8 index = 0; index < kSecondaryWheelSlotCount; ++index) {
        const s16 difference = static_cast<s16>(
            stickAngle - g_secondaryWheelSlotAngles[index]);
        const int distance = std::abs(static_cast<int>(difference));
        if (distance < smallestDifference) {
            smallestDifference = distance;
            closestSlot = index;
        }
    }
    return closestSlot;
}

void position_secondary_cursor(dMenu_Ring_c* ring);

void move_secondary_ring(dMenu_Ring_c* ring) {
    if (ring->mStatus == dMenu_Ring_c::STATUS_WAIT &&
        ring->mpStick != nullptr &&
        ring->mpStick->getValueStick() >= 0.75f)
    {
        const u8 newSlot = closest_secondary_wheel_slot(ring->mpStick->getAngleStick());
        if (newSlot != ring->mCurrentSlot) {
            ring->mCurrentSlot = newSlot;
            ring->field_0x670 = g_secondaryWheelSlotAngles[newSlot];
        }
    }

    if (ring->mCurrentSlot < kSecondaryWheelSlotCount) {
        const u8 selected = g_secondaryWheelSlotIds[ring->mCurrentSlot];
        if (mDoCPd_c::getTrigX(PAD_1) != 0) {
            g_secondarySelectedItems[0] = selected;
            save_secondary_assignment(0, selected);
            ring->mXButtonSlot = ring->mCurrentSlot;
            ring->setSelectItem(0, secondaryDisplayItemForSlot(selected));
        }
        if (mDoCPd_c::getTrigY(PAD_1) != 0) {
            g_secondarySelectedItems[1] = selected;
            save_secondary_assignment(1, selected);
            ring->mYButtonSlot = ring->mCurrentSlot;
            ring->setSelectItem(1, secondaryDisplayItemForSlot(selected));
        }
        for (int button = 0; button < 2; ++button) {
            if (secondaryTunicAssigned(button)) {
                for (auto* icon : ring->mpSelectItemTex[button]) {
                    tint_tunic_icon(icon);
                }
            }
        }
        if (!secondaryTunicAssigned(0) && !secondaryTunicAssigned(1)) {
            g_goronTunicEquipped = false;
            g_goronLavaContactFrames = 0;
        }
        if (!secondaryHoverBootsAssigned(0) && !secondaryHoverBootsAssigned(1)) {
            clear_hover_water_ground_plane();
            g_hoverBootsEquipped = false;
            g_hoverGroundHeightValid = false;
            g_hoverLandingSequenceActive = false;
            g_hoverAtSurface = false;
            g_hoverLandingFrames = 0;
            g_hoverRippleEmitter = 0;
            stop_hover_magic_effects();
        }
    }
    position_secondary_cursor(ring);
    set_secondary_ring_name(ring);
}

void position_secondary_cursor(dMenu_Ring_c* ring) {
    if (ring->mCurrentSlot >= kSecondaryWheelSlotCount) {
        return;
    }

    const int index = ring->mCurrentSlot;
    ring->mpDrawCursor->setPos(g_secondaryWheelSlotPosX[index] + ring->mCenterPosX,
        g_secondaryWheelSlotPosY[index] + ring->mCenterPosY);
    ring->mpDrawCursor->setParam(
        kSecondaryCursorScale, kSecondaryCursorScale, 0.1f, 0.6f, 0.5f);
}

HookAction open_secondary_ring(ModContext*, void* args, void* retval, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!isSecondaryRing(ring)) {
        return HOOK_CONTINUE;
    }

    if (ring->mOpenCloseFrames == 0) {
        dMeter2Info_set2DVibrationM();
    }
    ++ring->mOpenCloseFrames;
    ring->mAlphaRate = static_cast<f32>(ring->mOpenCloseFrames) / g_ringHIO.mOpenFrames;
    if (ring->mRingOrigin == 0) {
        ring->mCenterPosX = 0.0f;
        ring->mCenterPosY = (1.0f - ring->mAlphaRate) * FB_HEIGHT_BASE;
    } else if (ring->mRingOrigin == 2) {
        ring->mCenterPosX = 0.0f;
        ring->mCenterPosY = (1.0f - ring->mAlphaRate) * -FB_HEIGHT_BASE;
    } else if (ring->mRingOrigin == 3) {
        ring->mCenterPosX = (1.0f - ring->mAlphaRate) * FB_WIDTH_BASE;
        ring->mCenterPosY = 0.0f;
    } else {
        ring->mCenterPosX = (1.0f - ring->mAlphaRate) * -FB_WIDTH_BASE;
        ring->mCenterPosY = 0.0f;
    }
    const bool opened = ring->mOpenCloseFrames >= g_ringHIO.mOpenFrames;
    if (opened) {
        ring->mOpenCloseFrames = g_ringHIO.mCloseFrames;
        ring->mAlphaRate = 1.0f;
        ring->mCenterPosX = 0.0f;
        ring->mCenterPosY = 0.0f;
    }
    position_secondary_cursor(ring);
    *static_cast<bool*>(retval) = opened;
    return HOOK_SKIP_ORIGINAL;
}

HookAction close_secondary_ring(ModContext*, void* args, void* retval, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!isSecondaryRing(ring)) {
        return HOOK_CONTINUE;
    }

    if (ring->field_0x674[0] != 0 || ring->field_0x674[1] != 0 ||
        ring->field_0x674[2] != 0 || ring->field_0x674[3] != 0)
    {
        *static_cast<bool*>(retval) = false;
        return HOOK_SKIP_ORIGINAL;
    }

    --ring->mOpenCloseFrames;
    ring->mAlphaRate = static_cast<f32>(ring->mOpenCloseFrames) / g_ringHIO.mCloseFrames;
    const bool closed = ring->mOpenCloseFrames <= 0;
    if (closed) {
        ring->mOpenCloseFrames = 0;
        ring->mAlphaRate = 0.0f;
    }
    if (ring->mRingOrigin == 0) {
        ring->mCenterPosX = 0.0f;
        ring->mCenterPosY = (1.0f - ring->mAlphaRate) * -FB_HEIGHT_BASE;
    } else if (ring->mRingOrigin == 2) {
        ring->mCenterPosX = 0.0f;
        ring->mCenterPosY = (1.0f - ring->mAlphaRate) * FB_HEIGHT_BASE;
    } else if (ring->mRingOrigin == 3) {
        ring->mCenterPosX = (1.0f - ring->mAlphaRate) * -FB_WIDTH_BASE;
        ring->mCenterPosY = 0.0f;
    } else {
        ring->mCenterPosX = (1.0f - ring->mAlphaRate) * FB_WIDTH_BASE;
        ring->mCenterPosY = 0.0f;
    }
    position_secondary_cursor(ring);
    *static_cast<bool*>(retval) = closed;
    return HOOK_SKIP_ORIGINAL;
}

void draw_secondary_ring(dMenu_Ring_c* ring) {
    J2DGrafContext* grafPort = dComIfGp_getCurrentGrafPort();
    grafPort->setup2D();
    if (ring->mRingPosX != g_ringHIO.mRingPosX ||
        ring->mRingPosY != g_ringHIO.mRingPosY)
    {
        ring->mRingPosX = g_ringHIO.mRingPosX;
        ring->mRingPosY = g_ringHIO.mRingPosY;
        ring->mpCircle->paneTrans(ring->mRingPosX, ring->mRingPosY);
    }
    if (ring->mRingScaleH != g_ringHIO.mRingScaleH ||
        ring->mRingScaleV != g_ringHIO.mRingScaleV)
    {
        ring->mRingScaleH = g_ringHIO.mRingScaleH;
        ring->mRingScaleV = g_ringHIO.mRingScaleV;
        ring->mpCircle->scale(ring->mRingScaleH, ring->mRingScaleV);
    }
    ring->mpSpotParent->setAlphaRate(ring->mAlphaRate * g_ringHIO.mOverlayAlpha);
    ring->mpSpotScreen->draw(0.0f, 0.0f, grafPort);
    ring->mpCenterParent->setAlphaRate(ring->mAlphaRate);
    ring->mpCenterScreen->draw(ring->mCenterPosX, ring->mCenterPosY, grafPort);
    ring->mpMessageParent->setAlphaRate(ring->mAlphaRate);
    ring->mpScreen->draw(ring->mCenterPosX, ring->mCenterPosY, grafPort);

    std::array<std::array<J2DPicture*, 3>, MAX_ITEM_SLOTS> savedItemTextures{};
    for (int index = 0; index < MAX_ITEM_SLOTS; ++index) {
        for (int texture = 0; texture < 3; ++texture) {
            savedItemTextures[index][texture] = ring->mpItemTex[index][texture];
            ring->mpItemTex[index][texture] = nullptr;
        }
    }

    ring->drawItem();
    const u8 savedCurrentSlot = ring->mCurrentSlot;
    const f32 savedLastSlotX = ring->mItemSlotPosX[MAX_ITEM_SLOTS - 1];
    const f32 savedLastSlotY = ring->mItemSlotPosY[MAX_ITEM_SLOTS - 1];
    if (ring->mCurrentSlot >= MAX_ITEM_SLOTS) {
        ring->mCurrentSlot = MAX_ITEM_SLOTS - 1;
        ring->mItemSlotPosX[MAX_ITEM_SLOTS - 1] =
            g_secondaryWheelSlotPosX[savedCurrentSlot];
        ring->mItemSlotPosY[MAX_ITEM_SLOTS - 1] =
            g_secondaryWheelSlotPosY[savedCurrentSlot];
    }
    ring->drawItem2();
    ring->mCurrentSlot = savedCurrentSlot;
    ring->mItemSlotPosX[MAX_ITEM_SLOTS - 1] = savedLastSlotX;
    ring->mItemSlotPosY[MAX_ITEM_SLOTS - 1] = savedLastSlotY;
    for (int index = 0; index < MAX_ITEM_SLOTS; ++index) {
        for (int texture = 0; texture < 3; ++texture) {
            ring->mpItemTex[index][texture] = savedItemTextures[index][texture];
        }
    }

    for (u8 index = 0; index < kSecondaryWheelSlotCount; ++index) {
        const f32 x = g_secondaryWheelSlotPosX[index] - 24.0f + ring->mCenterPosX;
        const f32 y = g_secondaryWheelSlotPosY[index] - 24.0f + ring->mCenterPosY;
        if (index == MAX_ITEM_SLOTS) {
            if (g_sitWheelItemTex != nullptr) {
                constexpr f32 iconSize = 36.0f;
                g_sitWheelItemTex->setAlpha(ring->mAlphaRate * 255.0f);
                g_sitWheelItemTex->draw(
                    x + (48.0f - iconSize) * 0.5f,
                    y + (48.0f - iconSize) * 0.5f,
                    iconSize, iconSize, 0, 0, 0);
            }
            continue;
        }
        if (index == MAX_ITEM_SLOTS + 1) {
            if (g_tunicWheelItemBuf[0] == nullptr) {
                continue;
            }
            const f32 scale =
                dItem_data::getTexScale(kGoronTunicDisplayItem) / 100.0f * kSecondaryIconScale;
            const f32 width = g_tunicWheelItemBuf[0]->width / 48.0f * scale * 48.0f;
            const f32 height = g_tunicWheelItemBuf[0]->height / 48.0f * scale * 48.0f;
            for (auto* picture : g_tunicWheelItemTex) {
                if (picture == nullptr) {
                    continue;
                }
                picture->setAlpha(ring->mAlphaRate * 255.0f);
                picture->draw(x + (48.0f - width) * 0.5f,
                    y + (48.0f - height) * 0.5f, width, height, 0, 0, 0);
            }
            continue;
        }

        for (int texture = 0; texture < 3; ++texture) {
            auto* picture = ring->mpItemTex[index][texture];
            if (picture == nullptr) {
                continue;
            }
            picture->setAlpha(ring->mAlphaRate * 255.0f);
            const f32 width = ring->mItemSlotParam1[index] * 48.0f;
            const f32 height = ring->mItemSlotParam2[index] * 48.0f;
            picture->draw(x + (48.0f - width) * 0.5f,
                y + (48.0f - height) * 0.5f, width, height, 0, 0, 0);
        }
    }

    constexpr unsigned long long buttonPanes[] = {
        MULTI_CHAR('x_btn_n'), MULTI_CHAR('y_btn_n')};
    for (int button = 0; button < 2; ++button) {
        if (!secondaryItemAssigned(button)) {
            continue;
        }
        auto* pane = ring->mpScreen->search(buttonPanes[button]);
        if (pane == nullptr) {
            continue;
        }
        CPaneMgr paneMgr;
        const Vec center = paneMgr.getGlobalVtxCenter(pane, true, 0);
        for (int texture = 0; texture < ring->field_0x686[button]; ++texture) {
            auto* picture = ring->mpSelectItemTex[button][texture];
            if (picture == nullptr) {
                continue;
            }
            picture->setAlpha(ring->mAlphaRate * 255.0f);
            const f32 width = ring->field_0x548[button] * 48.0f;
            const f32 height = ring->field_0x558[button] * 48.0f;
            picture->draw(center.x - width * 0.5f + ring->mCenterPosX,
                center.y - height * 0.5f + ring->mCenterPosY,
                width, height, 0, 0, 0);
        }
    }

    ring->mpDrawCursor->draw();
}

HookAction suppress_secondary_explain(ModContext*, void* args, void* retval, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (g_openingSecondaryRing || isSecondaryRing(ring)) {
        *static_cast<u8*>(retval) = 0;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

void name_secondary_item(ModContext*, void* args, void*, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!isSecondaryRing(ring) || ring->mCurrentSlot >= kSecondaryWheelSlotCount ||
        ring->mpCenterScreen == nullptr)
    {
        return;
    }

    constexpr unsigned long long names[] = {
        MULTI_CHAR('item_n04'), MULTI_CHAR('item_n05'), MULTI_CHAR('item_n06'),
        MULTI_CHAR('item_n07'), MULTI_CHAR('fitem_n1'), MULTI_CHAR('fitem_n2'),
        MULTI_CHAR('fitem_n3'), MULTI_CHAR('fitem_n4')};
    const char* itemName = secondaryItemNameForSlot(
        g_secondaryWheelSlotIds[ring->mCurrentSlot]);
    for (const auto nameId : names) {
        if (auto* name = static_cast<J2DTextBox*>(ring->mpCenterScreen->search(nameId))) {
            name->setString(0x40, itemName);
        }
    }
}

HookAction begin_key_wait(ModContext*, void*, void*, void*) {
    ++g_keyWaitDepth;
    return HOOK_CONTINUE;
}

void end_key_wait(ModContext*, void*, void*, void*) {
    if (g_keyWaitDepth > 0) {
        --g_keyWaitDepth;
    }
}

HookAction begin_map_input(ModContext*, void*, void*, void*) {
    ++g_mapInputDepth;
    return HOOK_CONTINUE;
}

void end_map_input(ModContext*, void*, void*, void*) {
    if (g_mapInputDepth > 0) {
        --g_mapInputDepth;
    }
}

HookAction remap_down_trigger(ModContext*, void*, void* retval, void*) {
    if (mapOpensDown() &&
        (g_keyWaitDepth > 0 || g_secondaryRing != nullptr))
    {
        *static_cast<BOOL*>(retval) = mDoCPd_c::getTrigLeft(PAD_1) != 0;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction remap_left_trigger(ModContext*, void*, void* retval, void*) {
    if (mapOpensDown() && g_keyWaitDepth > 0) {
        *static_cast<BOOL*>(retval) = false;
        return HOOK_SKIP_ORIGINAL;
    }
    if (mapOpensDown() && g_mapInputDepth > 0) {
        *static_cast<BOOL*>(retval) = mDoCPd_c::getTrigDown(PAD_1) != 0;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction prepare_secondary_wheel(ModContext*, void* args, void*, void*) {
    g_openingSecondaryRing = false;

    const bool secondaryPressed =
        wheelOpensDown() ? mDoCPd_c::getTrigDown(PAD_1) != 0 :
                           mDoCPd_c::getTrigLeft(PAD_1) != 0;
    const bool primaryPressed = mDoCPd_c::getTrigUp(PAD_1) != 0;
    if (g_keyWaitDepth > 0 && secondaryPressed && !primaryPressed) {
        g_openingSecondaryRing = true;
        mods::arg_ref<u8>(args, 1) = mapOpensDown() ? 1 : 2;
    }

    return HOOK_CONTINUE;
}

void finish_secondary_wheel(ModContext*, void* args, void*, void*) {
    auto* window = mods::arg<dMw_c*>(args, 0);
    if (g_openingSecondaryRing) {
        g_secondaryRing = window->mpMenuRing;
        setup_secondary_ring(g_secondaryRing);
    } else {
        g_primaryRingOpen = true;
        g_pendingPrimaryItemSync = true;
    }
    g_openingSecondaryRing = false;
}

HookAction prepare_ring_delete(ModContext*, void* args, void*, void*) {
    auto* window = mods::arg<dMw_c*>(args, 0);
    g_deletingSecondaryRing = isSecondaryRing(window->mpMenuRing);
    if (g_deletingSecondaryRing) {
        clear_sit_wheel_texture();
        clear_tunic_wheel_textures();
    }
    return HOOK_CONTINUE;
}

void clear_secondary_wheel(ModContext*, void*, void*, void*) {
    if (g_deletingSecondaryRing) {
        g_secondaryRing = nullptr;
        g_deletingSecondaryRing = false;
    } else if (g_pendingPrimaryItemSync) {
        dComIfGp_setSelectItem(0);
        dComIfGp_setSelectItem(1);
        g_pendingPrimaryItemSync = false;
        g_primaryRingOpen = false;
    }
}

HookAction handle_secondary_ring_move_end(ModContext*, void* args, void* retval, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!isSecondaryRing(ring)) {
        return HOOK_CONTINUE;
    }
    if (ring->mStatus != dMenu_Ring_c::STATUS_WAIT ||
        ring->mOldStatus == dMenu_Ring_c::STATUS_EXPLAIN_FORCE ||
        ring->mOldStatus == dMenu_Ring_c::STATUS_EXPLAIN)
    {
        *static_cast<bool*>(retval) = false;
        return HOOK_SKIP_ORIGINAL;
    }

    const bool closing = dMw_UP_TRIGGER() || dMw_DOWN_TRIGGER() || dMw_B_TRIGGER() ||
                         dMeter2Info_getWarpStatus() == 2 ||
                         dMeter2Info_getWarpStatus() == 1 ||
                         dMeter2Info_isTouchKeyCheck(0xe);
    if (closing) {
        if (mDoCPd_c::getTrigLeft(PAD_1) != 0) {
            ring->mRingOrigin = 1;
        } else if (mDoCPd_c::getTrigRight(PAD_1) != 0) {
            ring->mRingOrigin = 3;
        } else if (dMw_DOWN_TRIGGER()) {
            ring->mRingOrigin = 2;
        } else {
            ring->mRingOrigin = 0;
        }
        Z2GetAudioMgr()->seStart(
            Z2SE_ITEM_RING_OUT, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        dMeter2Info_set2DVibrationM();
    }
    *static_cast<bool*>(retval) = closing;
    return HOOK_SKIP_ORIGINAL;
}

HookAction handle_secondary_ring_move(ModContext*, void* args, void*, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!isSecondaryRing(ring)) {
        return HOOK_CONTINUE;
    }
    move_secondary_ring(ring);
    return HOOK_SKIP_ORIGINAL;
}

HookAction handle_secondary_ring_draw(ModContext*, void* args, void*, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!isSecondaryRing(ring)) {
        return HOOK_CONTINUE;
    }
    draw_secondary_ring(ring);
    return HOOK_SKIP_ORIGINAL;
}

void replace_secondary_custom_icon(ModContext*, void* args, void*, void*) {
    if ((g_secondaryIconContext != SecondaryIconContext::Sit &&
         g_secondaryIconContext != SecondaryIconContext::HoverBoots &&
         g_secondaryIconContext != SecondaryIconContext::DarkLink) ||
        mods::arg<u8>(args, 0) != dItemNo_WOOD_STICK_e)
    {
        return;
    }

    const char* resourceName =
        g_secondaryIconContext == SecondaryIconContext::Sit
            ? kSitIconResource
            : (g_secondaryIconContext == SecondaryIconContext::DarkLink
                   ? kDarkLinkIconResource
                   : kHoverBootsIconResource);
    ResourceBuffer icon = RESOURCE_BUFFER_INIT;
    const ModResult result = svc_resource->load(mod_ctx, resourceName, &icon);
    if (result != MOD_OK || icon.data == nullptr || icon.size == 0) {
        svc_log->error(mod_ctx, "failed to load a custom secondary item icon");
        svc_resource->free(mod_ctx, &icon);
        return;
    }
    if (icon.size > kItemTextureBufferSize) {
        svc_log->error(mod_ctx, "custom secondary item icon exceeds the texture buffer");
        svc_resource->free(mod_ctx, &icon);
        return;
    }

    for (int index : {1, 3, 5}) {
        void* textureBuffer = mods::arg<void*>(args, index);
        if (textureBuffer != nullptr) {
            std::memcpy(textureBuffer, icon.data, icon.size);
        }
    }
    svc_resource->free(mod_ctx, &icon);
}

HookAction apply_hawk_grass_equip_icon(ModContext*, void* args, void*, void*) {
    if (g_secondaryIconContext == SecondaryIconContext::HawkGrass &&
        mods::arg<u8>(args, 0) == dItemNo_WOOD_STICK_e)
    {
        mods::arg_ref<int>(args, 9) = kHawkGrassIconTexture;
    }
    return HOOK_CONTINUE;
}

HookAction prepare_secondary_equip_icon(ModContext*, void* args, void*, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    const int button = mods::arg<int>(args, 1);
    if (!isSecondaryRing(ring) || button < 0 || button > 1) {
        return HOOK_CONTINUE;
    }

    const u8 slot = g_secondarySelectedItems[button];
    g_secondaryIconContext = secondaryIconContextForSlot(slot);
    if (secondaryItemAssigned(button)) {
        mods::arg_ref<u8>(args, 2) = secondaryDisplayItemForSlot(slot);
    }
    return HOOK_CONTINUE;
}

void finish_secondary_equip_icon(ModContext*, void*, void*, void*) {
    g_secondaryIconContext = SecondaryIconContext::None;
}

HookAction prepare_hover_boots_hud_icon(ModContext*, void* args, void*, void*) {
    const int button = mods::arg<int>(args, 1);
    if (button < 0 || button > 1 || !secondaryItemAssigned(button)) {
        g_secondaryIconContext = SecondaryIconContext::None;
        return HOOK_CONTINUE;
    }

    const u8 slot = g_secondarySelectedItems[button];
    g_secondaryIconContext = secondaryIconContextForSlot(slot);
    if (g_secondaryIconContext != SecondaryIconContext::Sit &&
        g_secondaryIconContext != SecondaryIconContext::HoverBoots &&
        g_secondaryIconContext != SecondaryIconContext::DarkLink)
    {
        mods::arg_ref<u8>(args, 2) = secondaryDisplayItemForSlot(slot);
        return HOOK_CONTINUE;
    }

    auto* meter = mods::arg<dMeter2Draw_c*>(args, 0);
    if (meter == nullptr || meter->mpItemXY[button] == nullptr ||
        meter->mpItemXYPane[button] == nullptr)
    {
        svc_log->error(mod_ctx, "failed to access the X/Y HUD panes for a custom item");
        return HOOK_SKIP_ORIGINAL;
    }

    const char* resourceName =
        g_secondaryIconContext == SecondaryIconContext::Sit
            ? kSitIconResource
            : (g_secondaryIconContext == SecondaryIconContext::DarkLink
                   ? kDarkLinkIconResource
                   : kHoverBootsIconResource);
    ResourceBuffer icon = RESOURCE_BUFFER_INIT;
    const ModResult result = svc_resource->load(mod_ctx, resourceName, &icon);
    if (result != MOD_OK || icon.data == nullptr || icon.size == 0) {
        svc_log->error(mod_ctx, "failed to load a custom X/Y HUD icon");
        svc_resource->free(mod_ctx, &icon);
        return HOOK_SKIP_ORIGINAL;
    }
    if (icon.size < sizeof(ResTIMG) || icon.size > kItemTextureBufferSize) {
        svc_log->error(mod_ctx, "custom X/Y HUD icon has an invalid texture size");
        svc_resource->free(mod_ctx, &icon);
        return HOOK_SKIP_ORIGINAL;
    }

    const u8 textureIndex = meter->field_0x76c[button] == 0 ? 1 : 0;
    meter->field_0x76c[button] = textureIndex;
    ResTIMG* texture = meter->mpItemXYTex[button][textureIndex][0];
    if (texture == nullptr) {
        svc_log->error(mod_ctx, "failed to access the custom X/Y HUD texture buffer");
        svc_resource->free(mod_ctx, &icon);
        return HOOK_SKIP_ORIGINAL;
    }

    std::memcpy(texture, icon.data, icon.size);
    svc_resource->free(mod_ctx, &icon);
    if (texture->width == 0 || texture->height == 0) {
        svc_log->error(mod_ctx, "Hover Boots X/Y HUD icon has invalid dimensions");
        return HOOK_SKIP_ORIGINAL;
    }
    DCStoreRangeNoSync(texture, kItemTextureBufferSize);

    auto* picture = static_cast<J2DPicture*>(meter->mpItemXY[button]->getPanePtr());
    if (picture == nullptr || picture->changeTexture(texture, 0) == nullptr) {
        svc_log->error(mod_ctx, "failed to apply the custom X/Y HUD icon");
        return HOOK_SKIP_ORIGINAL;
    }
    picture->setBlackWhite(
        JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 255, 255, 255));
    meter->mpItemXYPane[button]->hide();

    const f32 customIconScale =
        g_secondaryIconContext == SecondaryIconContext::DarkLink ? 1.5f : 1.0f;
    const f32 scale = (g_drawHIO.mItemScaleAdjustON
                           ? g_drawHIO.mItemScalePercent / 100.0f
                           : 1.0f) * customIconScale;
    meter->field_0x6c4[button] =
        scale * ((texture->width * meter->mpItemXY[button]->getInitSizeX()) / 48.0f);
    meter->field_0x6d0[button] =
        scale * ((texture->height * meter->mpItemXY[button]->getInitSizeY()) / 48.0f);
    meter->field_0x6ac[button] =
        (meter->mpItemXY[button]->getInitSizeX() - meter->field_0x6c4[button]) * 0.5f;
    meter->field_0x6b8[button] =
        (meter->mpItemXY[button]->getInitSizeY() - meter->field_0x6d0[button]) * 0.5f;
    meter->mpItemXY[button]->resize(meter->field_0x6c4[button], meter->field_0x6d0[button]);
    meter->mpItemXY[button]->paneTrans(
        meter->mItemParams[button].pos_x + meter->field_0x6ac[button],
        meter->mItemParams[button].pos_y + meter->field_0x6b8[button]);
    meter->mpItemXYPane[button]->resize(
        meter->field_0x6c4[button], meter->field_0x6d0[button]);

    return HOOK_SKIP_ORIGINAL;
}

void refresh_secondary_hud_assignments(ModContext*, void* args, void*, void*) {
    auto* meter = mods::arg<dMeter2_c*>(args, 0);
    if (meter == nullptr || meter->mpMeterDraw == nullptr) {
        return;
    }

    if (g_secondaryHudOwner != meter) {
        g_secondaryHudOwner = meter;
        g_secondaryHudAssignmentsInitialized = false;
    }

    if (!g_secondaryHudAssignmentsInitialized) {
        g_secondaryHudAssignments = g_secondarySelectedItems;
        for (int button = 0; button < MAX_SELECT_ITEM; ++button) {
            g_secondaryHudRefreshPending[button] = secondaryItemAssigned(button);
        }
        g_secondaryHudAssignmentsInitialized = true;
        return;
    }

    for (int button = 0; button < MAX_SELECT_ITEM; ++button) {
        const u8 slot = g_secondarySelectedItems[button];
        if (g_secondaryHudAssignments[button] != slot) {
            g_secondaryHudAssignments[button] = slot;
            g_secondaryHudRefreshPending[button] = true;
        }
        if (!g_secondaryHudRefreshPending[button] || meter->field_0x128 != 0 ||
            meter->mpMeterDraw->mpItemXY[button] == nullptr ||
            meter->mpMeterDraw->mpItemXYPane[button] == nullptr)
        {
            continue;
        }

        const u8 item = secondaryItemAssigned(button)
                            ? secondaryDisplayItemForSlot(slot)
                            : meter->mItemStatus[button * 2];
        const u8 status = meter->mItemStatus[button * 2 + 1];
        meter->mpMeterDraw->drawButtonXY(
            button, item, status, true, false);
        g_secondaryHudRefreshPending[button] = false;
    }
}

void clear_secondary_assignment_for_vanilla_ring(ModContext*, void* args, void*, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (ring == nullptr || isSecondaryRing(ring)) {
        return;
    }

    const int button = ring->field_0x6b3;
    if (button < 0 || button > 1 || !secondaryItemAssigned(button)) {
        return;
    }

    g_secondarySelectedItems[button] = dItemNo_NONE_e;
    save_secondary_assignment(button, dItemNo_NONE_e);

    if (!secondaryTunicAssigned(0) && !secondaryTunicAssigned(1)) {
        g_goronTunicEquipped = false;
    }
    if (!secondaryHoverBootsAssigned(0) && !secondaryHoverBootsAssigned(1)) {
        disable_hover_boots();
    }
}

void finish_hover_boots_hud_icon(ModContext*, void*, void*, void*) {
    g_secondaryIconContext = SecondaryIconContext::None;
}

HookAction prevent_lava_return(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink != nullptr && hoverBootsAirwalkingEnabled() && g_hoverBootsEquipped &&
        g_hoverGroundHeightValid &&
        !alink->mLinkAcch.ChkGroundHit() && !alink->checkEventRun() &&
        !alink->checkModeFlg(daAlink_c::MODE_SWIMMING))
    {
        if (alink->current.pos.y < g_hoverGroundHeight) {
            alink->current.pos.y = g_hoverGroundHeight;
            alink->speed.y = 0.0f;
        }
        *static_cast<BOOL*>(retval) = TRUE;
        return HOOK_SKIP_ORIGINAL;
    }

    if (!is_lava_polygon_contact(alink)) {
        return HOOK_CONTINUE;
    }

    if (infiniteLavaWalkEnabled() && g_goronTunicEquipped &&
        stabilize_lava_surface_contact(alink, true))
    {
        *static_cast<BOOL*>(retval) = TRUE;
        return HOOK_SKIP_ORIGINAL;
    }

    if (alink->mLinkAcch.ChkGroundHit() && alink->mGndPolyAtt1 == 2) {
        // Heavy armor promotes lava's sink attribute to quicksand's value,
        // whose return initializer assumes a sand-only transform buffer.
        alink->mGndPolyAtt1 = 1;
    }

    return HOOK_CONTINUE;
}

HookAction delay_goron_lava_return(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    const BOOL isSandReturn = mods::arg<BOOL>(args, 1);
    if (alink != nullptr && !isSandReturn && g_goronTunicEquipped &&
        !infiniteLavaWalkEnabled() && is_goron_lava_surface_contact(alink) &&
        g_goronLavaContactFrames < kGoronLavaReturnDelayFrames)
    {
        *static_cast<int*>(retval) = 0;
        return HOOK_SKIP_ORIGINAL;
    }

    return HOOK_CONTINUE;
}

void maintain_hover_height(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || !hoverBootsAirwalkingEnabled() || !g_hoverBootsEquipped ||
        alink->checkEventRun() ||
        alink->checkModeFlg(daAlink_c::MODE_SWIMMING))
    {
        return;
    }

    if (g_hoverBootsTransitionPending &&
        alink->mProcID == daAlink_c::PROC_BOOTS_EQUIP)
    {
        return;
    }

    if (g_hoverLandingSequenceActive && alink->mProcID == daAlink_c::PROC_LAND) {
        return;
    }

    if (g_sitActionState == SitActionState::Seated && is_hover_boots_hovering(alink)) {
        alink->current.pos.y = g_hoverGroundHeight;
        alink->speed.y = 0.0f;
        g_hoverLandingSequenceActive = false;
        return;
    }

    if (alink->mLinkAcch.ChkGroundHit() &&
        (!g_hoverGroundHeightValid || alink->current.pos.y >= g_hoverGroundHeight))
    {
        g_hoverGroundHeight = alink->current.pos.y;
        g_hoverGroundHeightValid = true;
        g_hoverLandingSequenceActive = false;
        g_hoverAtSurface = false;
        g_hoverLandingFrames = 0;
    }

    if (alink->speed.y > 0.0f ||
        (g_hoverGroundHeightValid && alink->current.pos.y > g_hoverGroundHeight))
    {
        g_hoverLandingSequenceActive = false;
        g_hoverAtSurface = false;
        g_hoverLandingFrames = 0;
    }

    if (g_hoverGroundHeightValid && alink->current.pos.y <= g_hoverGroundHeight &&
        alink->speed.y <= 0.0f)
    {
        alink->current.pos.y = g_hoverGroundHeight;
        alink->speed.y = 0.0f;
        if (!alink->mLinkAcch.ChkGroundHit() && !g_hoverAtSurface) {
            const f32 fallDistance = 0.01f * (alink->mFallHeight - alink->current.pos.y);
            alink->procLandInit(fallDistance);
            g_hoverLandingSequenceActive = true;
            g_hoverAtSurface = true;
            g_hoverLandingFrames = 0;
        }
    }
}

HookAction handle_hover_airborne_action(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || !hoverBootsAirwalkingEnabled() || !g_hoverBootsEquipped ||
        !g_hoverGroundHeightValid ||
        alink->checkEventRun() || alink->checkModeFlg(daAlink_c::MODE_SWIMMING) ||
        alink->checkNoResetFlg0(daAlink_c::FLG0_WATER_IN_MOVE) ||
        alink->mLinkAcch.ChkGroundHit())
    {
        return HOOK_CONTINUE;
    }

    *static_cast<BOOL*>(retval) = TRUE;
    return HOOK_SKIP_ORIGINAL;
}

void continue_hover_landing(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || !hoverBootsAirwalkingEnabled() || !g_hoverBootsEquipped ||
        !g_hoverLandingSequenceActive ||
        alink->mLinkAcch.ChkGroundHit() || alink->mProcID != daAlink_c::PROC_LAND)
    {
        return;
    }

    if (g_hoverLandingFrames < kHoverLandingRecoveryFrames) {
        ++g_hoverLandingFrames;
    }
    const bool landingAnimationFinished =
        alink->mUnderFrameCtrl[0].getEndFlg() != 0 &&
        alink->mUnderFrameCtrl[0].getNowSetFlg() == 0;
    if (!landingAnimationFinished && g_hoverLandingFrames < kHoverLandingRecoveryFrames) {
        return;
    }

    g_hoverLandingSequenceActive = false;
    g_hoverLandingFrames = 0;
    alink->procMoveInit();
}

HookAction suppress_hover_ledge_grab(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || !hoverBootsAirwalkingEnabled() || !g_hoverBootsEquipped ||
        !g_hoverGroundHeightValid ||
        alink->checkEventRun() || alink->checkModeFlg(daAlink_c::MODE_SWIMMING) ||
        alink->mLinkAcch.ChkGroundHit())
    {
        return HOOK_CONTINUE;
    }

    *static_cast<int*>(retval) = 0;
    return HOOK_SKIP_ORIGINAL;
}

void update_hover_magic_after_model_calc(ModContext*, void* args, void*, void*) {
    update_hover_magic_effect(mods::arg<daAlink_c*>(args, 0));
}

HookAction offset_sit_model_forward_before_calc(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    auto* model = mods::arg<J3DModel*>(args, 1);
    if (alink == nullptr || model == nullptr || model != alink->mpLinkModel ||
        g_sitActionState != SitActionState::Seated ||
        alink->mProcID != daAlink_c::PROC_WAIT)
    {
        return HOOK_CONTINUE;
    }

    MtxP baseMatrix = model->getBaseTRMtx();
    baseMatrix[0][3] =
        alink->current.pos.x + cM_ssin(alink->shape_angle.y) * kSitModelForwardOffset;
    baseMatrix[2][3] =
        alink->current.pos.z + cM_scos(alink->shape_angle.y) * kSitModelForwardOffset;
    return HOOK_CONTINUE;
}

void treat_lava_as_surface(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!infiniteLavaWalkEnabled() || !g_goronTunicEquipped || alink == nullptr) {
        return;
    }

    stabilize_lava_surface_contact(alink, true);
}

bool is_hover_water_walk_active(const daAlink_c* alink) {
    if (!hoverBootsWaterWalkingEnabled() || !g_hoverBootsEquipped || alink == nullptr ||
        alink->field_0x2fbc == 6 || alink->checkEventRun() ||
        !g_hoverWaterGroundPlaneActive)
    {
        return false;
    }

    const f32 surfaceY = alink->mWaterY != -G_CM3D_F_INF
                             ? alink->mWaterY
                             : g_hoverWaterSurfaceHeight;
    return (g_hoverWaterUnderlyingGroundHeight == -G_CM3D_F_INF ||
            g_hoverWaterUnderlyingGroundHeight < surfaceY - 1.0f);
}

bool is_hover_water_walk_contact(const daAlink_c* alink) {
    if (!is_hover_water_walk_active(alink)) {
        return false;
    }

    const f32 surfaceY = alink->mWaterY != -G_CM3D_F_INF
                             ? alink->mWaterY
                             : g_hoverWaterSurfaceHeight;
    return alink->mLinkAcch.ChkGroundHit() &&
           alink->current.pos.y <= surfaceY + 1.0f;
}

f32 get_hover_water_surface_y(const daAlink_c* alink) {
    if (alink->mWaterY != -G_CM3D_F_INF) {
        return alink->mWaterY;
    }
    return g_hoverWaterSurfaceHeight;
}

bool is_hover_water_walk_candidate(const daAlink_c* alink) {
    if (!hoverBootsWaterWalkingEnabled() || !g_hoverBootsEquipped || alink == nullptr ||
        alink->checkEventRun() || alink->field_0x2fbc == 6 ||
        (alink->mWaterY == -G_CM3D_F_INF &&
         (!g_hoverWaterGroundPlaneActive ||
          g_hoverWaterMissFrames > kHoverWaterMissToleranceFrames)))
    {
        return false;
    }

    const f32 surfaceY = get_hover_water_surface_y(alink);
    const f32 groundY = alink->mLinkAcch.GetGroundH();
    if (groundY != -G_CM3D_F_INF && groundY >= surfaceY - 1.0f) {
        return false;
    }

    return alink->current.pos.y <= surfaceY + alink->mHeight &&
           !(alink->speed.y > 0.0f && alink->current.pos.y >= surfaceY);
}

HookAction flatten_hover_water_ground_angle(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!is_hover_water_walk_active(alink)) {
        return HOOK_CONTINUE;
    }

    *static_cast<s16*>(retval) = 0;
    return HOOK_SKIP_ORIGINAL;
}

HookAction suppress_submerged_water_slide_polygon(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!is_hover_water_walk_active(alink)) {
        return HOOK_CONTINUE;
    }

    *static_cast<bool*>(retval) = false;
    return HOOK_SKIP_ORIGINAL;
}

HookAction suppress_submerged_water_slope(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!is_hover_water_walk_active(alink)) {
        return HOOK_CONTINUE;
    }

    *static_cast<BOOL*>(retval) = FALSE;
    return HOOK_SKIP_ORIGINAL;
}

HookAction suppress_submerged_water_step_offset(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!is_hover_water_walk_active(alink)) {
        return HOOK_CONTINUE;
    }

    alink->field_0x2b9c = 0.0f;
    alink->field_0x2b98 = 0.0f;
    return HOOK_SKIP_ORIGINAL;
}

HookAction skip_submerged_ice_slip(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    return is_hover_water_walk_active(alink) ? HOOK_SKIP_ORIGINAL : HOOK_CONTINUE;
}

HookAction keep_hover_boots_on_water_surface(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    return is_hover_water_walk_contact(alink) && alink->speed.y <= 0.0f
               ? HOOK_SKIP_ORIGINAL
               : HOOK_CONTINUE;
}

HookAction suppress_water_surface_restart(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!is_hover_water_walk_contact(alink)) {
        return HOOK_CONTINUE;
    }

    *static_cast<BOOL*>(retval) = FALSE;
    return HOOK_SKIP_ORIGINAL;
}

HookAction suppress_submerged_water_exit(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!is_hover_water_walk_contact(alink)) {
        return HOOK_CONTINUE;
    }

    *static_cast<int*>(retval) = 0;
    return HOOK_SKIP_ORIGINAL;
}

void treat_water_as_surface_with_hover_boots(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr) {
        return;
    }

    if (g_hoverWaterGroundPlaneActive && hoverBootsWaterWalkingEnabled() &&
        g_hoverBootsEquipped && alink->field_0x2fbc != 6 && !alink->checkEventRun())
    {
        if (alink->mWaterY != -G_CM3D_F_INF) {
            const f32 groundY = alink->mLinkAcch.GetGroundH();
            if (groundY != -G_CM3D_F_INF &&
                groundY >= alink->mWaterY - 1.0f)
            {
                clear_hover_water_ground_plane();
                return;
            }
            g_hoverWaterSurfaceHeight = alink->mWaterY;
            g_hoverWaterMissFrames = 0;
        } else if (g_hoverWaterMissFrames < kHoverWaterMissToleranceFrames) {
            ++g_hoverWaterMissFrames;
        } else {
            clear_hover_water_ground_plane();
        }
    }

    if (!is_hover_water_walk_candidate(alink)) {
        if (g_hoverWaterGroundPlaneActive && alink->speed.y > 0.0f &&
            alink->current.pos.y >= get_hover_water_surface_y(alink))
        {
            alink->mLinkAcch.ClrGroundHit();
            alink->mLinkAcch.ClrGroundFind();
            alink->mLinkAcch.ClrGroundLanding();
            alink->mLinkAcch.ClrGroundAway();
            alink->mLinkAcch.m_ground_h = -G_CM3D_F_INF;
        }
        clear_hover_water_ground_plane();
        return;
    }

    const f32 surfaceY = get_hover_water_surface_y(alink);
    const f32 groundY = alink->mLinkAcch.GetGroundH();
    if (groundY != -G_CM3D_F_INF && groundY < surfaceY - 1.0f) {
        g_hoverWaterUnderlyingGroundHeight = groundY;
    }
    alink->current.pos.y = surfaceY;
    alink->speed.y = 0.0f;
    alink->mLinkAcch.m_ground_h = surfaceY;
    alink->mLinkAcch.SetGroundHit();
    alink->mLinkAcch.SetGroundFind();
    alink->mLinkAcch.ClrGroundLanding();
    alink->mLinkAcch.ClrGroundAway();
    g_hoverWaterSurfaceHeight = surfaceY;
    g_hoverWaterGroundPlaneActive = true;
    g_hoverWaterMissFrames = 0;
    alink->offModeFlg(daAlink_c::MODE_SWIMMING);
    alink->mGndPolyAtt0 = 3;
    alink->mGndPolyAtt1 = 0;
    g_hoverGroundHeight = surfaceY;
    g_hoverGroundHeightValid = true;
    g_hoverLandingSequenceActive = false;
    g_hoverAtSurface = true;
    g_hoverLandingFrames = 0;
}

void maintain_lava_ground(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!is_lava_polygon_contact(alink)) {
        return;
    }

    if (infiniteLavaWalkEnabled() && g_goronTunicEquipped) {
        stabilize_lava_surface_contact(alink, true);
    } else if (g_goronTunicEquipped &&
               g_goronLavaContactFrames < kGoronLavaReturnDelayFrames &&
               stabilize_lava_surface_contact(alink, false))
    {
        if (alink->mGndPolyAtt1 == 2) {
            alink->mGndPolyAtt1 = 1;
        }
    } else if (alink->mLinkAcch.ChkGroundHit() && alink->mGndPolyAtt1 == 2) {
        alink->mGndPolyAtt1 = 1;
    }
}

void maintain_hover_water_surface(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!is_hover_water_walk_active(alink) || !alink->mLinkAcch.ChkGroundHit()) {
        return;
    }

    alink->mGndPolyAtt0 = 3;
    alink->mGndPolyAtt1 = 0;
    alink->mGroundCode = 3;
    alink->mGndPolySpecialCode = dBgW_SPCODE_NORMAL;
    alink->mLinkAcch.m_ground_h = get_hover_water_surface_y(alink);
}

HookAction land_on_lava_surface(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!is_lava_surface_contact(alink)) {
        return HOOK_CONTINUE;
    }

    const f32 fallDistance = 0.01f * (alink->mFallHeight - alink->current.pos.y);
    alink->procLandInit(fallDistance);
    *static_cast<BOOL*>(retval) = TRUE;
    return HOOK_SKIP_ORIGINAL;
}

HookAction prepare_lava_sinking(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!is_lava_surface_contact(alink)) {
        return HOOK_CONTINUE;
    }

    alink->mSinkShapeOffset = 0.0f;
    return HOOK_SKIP_ORIGINAL;
}

HookAction lower_sit_model_to_ground(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || g_sitActionState != SitActionState::Seated ||
        alink->mProcID != daAlink_c::PROC_WAIT ||
        !alink->mLinkAcch.ChkGroundHit())
    {
        return HOOK_CONTINUE;
    }

    alink->mSinkShapeOffset = kSitModelGroundOffset;
    return HOOK_SKIP_ORIGINAL;
}

HookAction suppress_lava_submersion(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (infiniteLavaWalkEnabled() && g_goronTunicEquipped &&
        is_lava_polygon_contact(alink))
    {
        *static_cast<BOOL*>(retval) = FALSE;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction suppress_lava_damage(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (infiniteLavaWalkEnabled() && g_goronTunicEquipped &&
        is_lava_polygon_contact(alink))
    {
        *static_cast<int*>(retval) = 0;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction suppress_twilight_ward_hit_stun(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || retval == nullptr || !twilightWardEnabled() ||
        !alink->checkMagicArmorWearAbility() || alink->checkEventRun())
    {
        return HOOK_CONTINUE;
    }

    *static_cast<int*>(retval) = 1;
    return HOOK_SKIP_ORIGINAL;
}

void collect_nearby_rupees(ModContext*, void* args, void*, void*) {
    if (!rupeeMagnetEnabled()) {
        return;
    }

    auto* item = mods::arg<daItem_c*>(args, 0);
    if (item == nullptr || item->getStatus() != daItem_c::STATUS_NORMAL_e ||
        !item->checkPlayerGet() || !is_rupee(item->getItemNo()))
    {
        return;
    }

    const f32 radius =
        kRupeeMagnetBaseRadius * static_cast<f32>(rupeeMagnetRadiusPercent()) / 100.0f;
    if (fopAcM_searchPlayerDistance(item) < radius) {
        item->itemGetNextExecute();
    }
}

void tint_clothing_material(J3DModelData* model, u16 index) {
    if (model == nullptr || index >= model->getMaterialNum()) {
        return;
    }

    auto* material = model->getMaterialNodePointer(index);
    auto* color = material != nullptr ? material->getTevColor(1) : nullptr;
    if (color == nullptr) {
        return;
    }

    color->r = 72;
    color->g = 0;
    color->b = 0;
}

void tint_goron_armor_indices(J3DModelData* model, const u16* indices,
                              size_t indexCount) {
    if (model == nullptr) {
        return;
    }
    for (size_t i = 0; i < indexCount; ++i) {
        tint_clothing_material(model, indices[i]);
    }
}

void tint_clothing_model(J3DModelData* model) {
    if (model == nullptr) {
        return;
    }

    static constexpr const char* tunicMaterials[] = {
        "al_upbody_m", "al_skirt_m", "al_lowbody_m", "ml_body_m", "ml_armor_m",
        "zl_up_armor_m", "al_cap_m", "ml_cap_m", "zl_cap_m"};
    auto* materialNames = model->getMaterialName();
    if (materialNames == nullptr) {
        return;
    }

    for (u16 i = 0; i < model->getMaterialNum(); ++i) {
        const char* name = materialNames->getName(i);
        if (name == nullptr) {
            continue;
        }

        if (std::none_of(std::begin(tunicMaterials), std::end(tunicMaterials),
                [name](const char* tunicMaterial) {
                    return std::strcmp(name, tunicMaterial) == 0;
                }))
        {
            continue;
        }

        tint_clothing_material(model, i);
    }
}

bool is_link_clothing_model(const daAlink_c* alink, J3DModelData* model) {
    return alink != nullptr && model != nullptr &&
           (model == alink->field_0x064C ||
            (alink->mpLinkFaceModel != nullptr &&
             model == alink->mpLinkFaceModel->getModelData()) ||
            (alink->mpLinkHatModel != nullptr &&
             model == alink->mpLinkHatModel->getModelData()));
}

void tint_goron_armor_model(daAlink_c* alink, J3DModelData* model) {
    if (!is_link_clothing_model(alink, model)) {
        return;
    }

    tint_clothing_model(model);
    if (model == alink->field_0x064C) {
        static constexpr u16 zoraBodyIndices[] = {13, 0, 1};
        static constexpr u16 magicBodyIndices[] = {11, 10, 9, 8, 6};
        static constexpr u16 casualBodyIndices[] = {7, 5};
        if (alink->checkZoraWearAbility()) {
            tint_goron_armor_indices(model, zoraBodyIndices,
                                     std::size(zoraBodyIndices));
        } else if (alink->checkMagicArmorWearAbility()) {
            tint_goron_armor_indices(model, magicBodyIndices,
                                     std::size(magicBodyIndices));
        } else if (daAlink_c::checkCasualWearFlg()) {
            tint_goron_armor_indices(model, casualBodyIndices,
                                     std::size(casualBodyIndices));
        }
    } else if (alink->mpLinkHatModel != nullptr &&
               model == alink->mpLinkHatModel->getModelData())
    {
        static constexpr u16 zoraHatIndices[] = {1};
        static constexpr u16 magicHatIndices[] = {2, 1};
        static constexpr u16 casualHatIndices[] = {0};
        if (alink->checkZoraWearAbility()) {
            tint_goron_armor_indices(model, zoraHatIndices,
                                     std::size(zoraHatIndices));
        } else if (alink->checkMagicArmorWearAbility()) {
            tint_goron_armor_indices(model, magicHatIndices,
                                     std::size(magicHatIndices));
        } else if (daAlink_c::checkCasualWearFlg()) {
            tint_goron_armor_indices(model, casualHatIndices,
                                     std::size(casualHatIndices));
        }
    }
}

void tint_goron_tunic(ModContext*, void* args, void*, void*) {
    if (!g_goronTunicEquipped) {
        return;
    }

    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || alink->getClothesChangeWaitTimer() != 0 ||
        alink->field_0x064C == nullptr)
    {
        return;
    }

    tint_goron_armor_model(alink, alink->field_0x064C);
    tint_goron_armor_model(alink, alink->mpLinkFaceModel != nullptr
                                      ? alink->mpLinkFaceModel->getModelData()
                                      : nullptr);
    tint_goron_armor_model(alink, alink->mpLinkHatModel != nullptr
                                      ? alink->mpLinkHatModel->getModelData()
                                      : nullptr);
}

void reapply_tunic_override_after_model_calc(ModContext*, void* args, void*, void*) {
    if (!g_goronTunicEquipped) {
        return;
    }

    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || alink->getClothesChangeWaitTimer() != 0) {
        return;
    }

    auto* model = mods::arg<J3DModel*>(args, 1);
    tint_goron_armor_model(alink, model == nullptr ? nullptr : model->getModelData());
}

void reapply_tunic_override_after_lighting(ModContext*, void* args, void*, void*) {
    if (!g_goronTunicEquipped) {
        return;
    }

    auto* alink = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    if (alink == nullptr || alink->getClothesChangeWaitTimer() != 0) {
        return;
    }

    auto* model = mods::arg<J3DModelData*>(args, 1);
    if (!is_link_clothing_model(alink, model)) {
        return;
    }
    tint_goron_armor_model(alink, model);
}

void restore_dark_link_colors() {
    g_darkLinkFormBlend = 0.0f;
    g_darkLinkDrawModelData = nullptr;
    g_darkLinkAppearanceModelData.fill(nullptr);
    g_darkLinkAppearanceModelDataCount = 0;
}

void play_dark_link_transition_effect(daAlink_c* alink, bool activating) {
    if (alink == nullptr) {
        return;
    }

    cXyz scale(1.0f, 1.0f, 1.0f);
    const u16 effectId = activating ? ID_ZI_S_LK_WARP_APP_A
                                    : ID_ZI_S_LK_WARP_DISAPP_A;
    dComIfGp_particle_set(dPa_RM(effectId), &alink->current.pos, nullptr, &scale);
}

f32 smoothstep(f32 edge0, f32 edge1, f32 value) {
    const f32 t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

bool contains_case_insensitive(const char* text, const char* needle) {
    if (text == nullptr || needle == nullptr || *needle == '\0') {
        return false;
    }

    for (const char* start = text; *start != '\0'; ++start) {
        const char* textChar = start;
        const char* needleChar = needle;
        while (*textChar != '\0' && *needleChar != '\0' &&
               std::tolower(static_cast<unsigned char>(*textChar)) ==
                   std::tolower(static_cast<unsigned char>(*needleChar)))
        {
            ++textChar;
            ++needleChar;
        }
        if (*needleChar == '\0') {
            return true;
        }
    }
    return false;
}

bool material_uses_pupil_texture(J3DModelData* modelData, J3DMaterial* material) {
    if (modelData == nullptr || material == nullptr) {
        return false;
    }

    J3DTexture* textures = modelData->getTexture();
    JUTNameTab* textureNames = modelData->getTextureName();
    J3DTevBlock* tevBlock = material->getTevBlock();
    if (textures == nullptr || textureNames == nullptr || tevBlock == nullptr) {
        return false;
    }

    const u8 stageCount = std::min<u8>(material->getTevStageNum(), 16);
    for (u8 stage = 0; stage < stageCount; ++stage) {
        J3DTevOrder* order = tevBlock->getTevOrder(stage);
        const u8 textureMap = order != nullptr ? order->getTexMap() : 0xFF;
        if (textureMap > 7) {
            continue;
        }

        const u16 textureIndex = tevBlock->getTexNo(textureMap);
        if (textureIndex >= textures->getNum()) {
            continue;
        }

        const char* textureName = textureNames->getName(textureIndex);
        if (contains_case_insensitive(textureName, "pupil") ||
            contains_case_insensitive(textureName, "iris") ||
            contains_case_insensitive(textureName, "hitomi"))
        {
            return true;
        }
    }
    return false;
}

void remember_dark_link_model_data(J3DModelData* modelData) {
    if (modelData == nullptr) {
        return;
    }

    for (u8 i = 0; i < g_darkLinkAppearanceModelDataCount; ++i) {
        if (g_darkLinkAppearanceModelData[i] == modelData) {
            return;
        }
    }
    if (g_darkLinkAppearanceModelDataCount < g_darkLinkAppearanceModelData.size()) {
        g_darkLinkAppearanceModelData[g_darkLinkAppearanceModelDataCount++] =
            modelData;
    }
}

void remember_dark_link_model_data(J3DModel* model) {
    remember_dark_link_model_data(
        model != nullptr ? model->getModelData() : nullptr);
}

bool is_dark_link_appearance_model_data(J3DModelData* modelData) {
    if (modelData == nullptr) {
        return false;
    }
    for (u8 i = 0; i < g_darkLinkAppearanceModelDataCount; ++i) {
        if (g_darkLinkAppearanceModelData[i] == modelData) {
            return true;
        }
    }
    return false;
}

HookAction mark_dark_link_model_for_draw(ModContext*, void* args, void*, void*) {
    g_darkLinkDrawModelData = nullptr;
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    auto* model = mods::arg<J3DModel*>(args, 1);
    if (alink == nullptr || model == nullptr) {
        return HOOK_CONTINUE;
    }

    g_darkLinkAppearanceModelData.fill(nullptr);
    g_darkLinkAppearanceModelDataCount = 0;
    if (!alink->checkWolf() && g_darkLinkFormBlend > 0.0f) {
        remember_dark_link_model_data(alink->mpLinkModel);
        remember_dark_link_model_data(alink->mpLinkFaceModel);
        remember_dark_link_model_data(alink->mpLinkHatModel);
        remember_dark_link_model_data(alink->mpLinkHandModel);
        remember_dark_link_model_data(alink->mpLinkBootModels[0]);
        remember_dark_link_model_data(alink->mpLinkBootModels[1]);
        remember_dark_link_model_data(alink->mShieldModel);
        remember_dark_link_model_data(alink->mSwordModel);
        remember_dark_link_model_data(alink->mSheathModel);
        remember_dark_link_model_data(alink->field_0x064C);
    }

    const bool isLinkAppearanceModel =
        model == alink->mpLinkModel || model == alink->mpLinkFaceModel ||
        model == alink->mpLinkHatModel || model == alink->mpLinkHandModel ||
        model == alink->mpLinkBootModels[0] || model == alink->mpLinkBootModels[1] ||
        model == alink->mShieldModel || model == alink->mSwordModel ||
        model == alink->mSheathModel ||
        (model->getModelData() != nullptr &&
         model->getModelData() == alink->field_0x064C);
    if (isLinkAppearanceModel) {
        if (!alink->checkWolf() && g_darkLinkFormBlend > 0.0f) {
            g_darkLinkDrawModelData = model->getModelData();
        }
    }
    return HOOK_CONTINUE;
}

void apply_dark_link_after_lighting(ModContext*, void* args, void*, void*) {
    auto* modelData = mods::arg<J3DModelData*>(args, 1);
    if (modelData == nullptr || modelData != g_darkLinkDrawModelData ||
        g_darkLinkFormBlend <= 0.0f)
    {
        return;
    }

    g_darkLinkDrawMaterialSnapshotCount = 0;
    auto* materialNames = modelData->getMaterialName();
    f32 modelMinY = G_CM3D_F_INF;
    f32 modelMaxY = -G_CM3D_F_INF;
    for (u16 i = 0; i < modelData->getMaterialNum(); ++i) {
        J3DMaterial* material = modelData->getMaterialNodePointer(i);
        J3DShape* shape = material != nullptr ? material->getShape() : nullptr;
        Vec* shapeMin = shape != nullptr ? shape->getMin() : nullptr;
        Vec* shapeMax = shape != nullptr ? shape->getMax() : nullptr;
        if (shapeMin == nullptr || shapeMax == nullptr) {
            continue;
        }
        modelMinY = std::min(modelMinY, shapeMin->y);
        modelMaxY = std::max(modelMaxY, shapeMax->y);
    }

    for (u16 i = 0; i < modelData->getMaterialNum(); ++i) {
        J3DMaterial* material = modelData->getMaterialNodePointer(i);
        if (material == nullptr) {
            continue;
        }

        J3DTevBlock* tevBlock = material->getTevBlock();
        const u8 stageCount = material->getTevStageNum();
        J3DTevStage* stage =
            tevBlock != nullptr && stageCount > 0
                ? tevBlock->getTevStage(stageCount - 1)
                : nullptr;
        J3DGXColorS10* tevColor = material->getTevColor(0);
        J3DGXColor* tevKColor = material->getTevKColor(0);
        if (g_darkLinkDrawMaterialSnapshotCount >=
            g_darkLinkDrawMaterialSnapshots.size())
        {
            svc_log->error(mod_ctx, "Dark Link Form exceeded its draw material capacity");
            break;
        }

        DarkLinkDrawMaterialSnapshot& snapshot =
            g_darkLinkDrawMaterialSnapshots[g_darkLinkDrawMaterialSnapshotCount++];
        snapshot.material = material;
        snapshot.stageIndex = stageCount > 0 ? stageCount - 1 : 0;
        if (stage != nullptr) {
            snapshot.tevKColorSel =
                tevBlock->getTevKColorSel(snapshot.stageIndex);
            snapshot.stage = *stage;
        }
        snapshot.hasTevColor = tevColor != nullptr;
        snapshot.hasTevKColor = tevKColor != nullptr;
        if (snapshot.hasTevColor) {
            snapshot.tevColor = *tevColor;
        }
        if (snapshot.hasTevKColor) {
            snapshot.tevKColor = *tevKColor;
        }

        const char* name = materialNames != nullptr ? materialNames->getName(i) : nullptr;
        snapshot.isPupilMaterial =
            contains_case_insensitive(name, "pupil") ||
            contains_case_insensitive(name, "iris") ||
            contains_case_insensitive(name, "hitomi") ||
            material_uses_pupil_texture(modelData, material);
        snapshot.blend = g_darkLinkFormBlend;
        J3DShape* shape = material->getShape();
        Vec* shapeMin = shape != nullptr ? shape->getMin() : nullptr;
        Vec* shapeMax = shape != nullptr ? shape->getMax() : nullptr;
        if (modelMaxY > modelMinY && shapeMin != nullptr && shapeMax != nullptr) {
            const f32 centerY = (shapeMin->y + shapeMax->y) * 0.5f;
            const f32 span = modelMaxY - modelMinY;
            const f32 band = std::max(12.0f, span * 0.04f);
            const f32 fragmentOffset =
                (static_cast<f32>((i * 37u + 11u) % 17u) / 16.0f - 0.5f) * band;
            const f32 progress =
                g_darkLinkFormActive ? g_darkLinkFormBlend : 1.0f - g_darkLinkFormBlend;
            if (g_darkLinkFormActive) {
                const f32 front = modelMaxY + band - progress * (span + 2.0f * band);
                snapshot.blend =
                    smoothstep(-band, band, centerY + fragmentOffset - front);
            } else {
                const f32 front = -band + progress * (span + 2.0f * band);
                snapshot.blend =
                    1.0f - smoothstep(
                               -band, band,
                               front - (centerY + fragmentOffset - modelMinY));
            }
        }
        if (g_darkLinkFormActive && g_darkLinkFormBlend >= 1.0f) {
            snapshot.blend = 1.0f;
        } else if (!g_darkLinkFormActive && g_darkLinkFormBlend <= 0.0f) {
            snapshot.blend = 0.0f;
        }
    }
}

void apply_dark_link_material_output(DarkLinkDrawMaterialSnapshot& snapshot) {
    J3DMaterial* material = snapshot.material;
    if (material == nullptr) {
        return;
    }

    J3DTevBlock* tevBlock = material->getTevBlock();
    J3DTevStage* stage =
        tevBlock != nullptr ? tevBlock->getTevStage(snapshot.stageIndex) : nullptr;
    if (stage == nullptr) {
        return;
    }

    if (snapshot.blend < 1.0f) {
        if (!snapshot.hasTevColor || !snapshot.hasTevKColor) {
            return;
        }

        J3DGXColorS10 factor{};
        const s16 brightness = static_cast<s16>((1.0f - snapshot.blend) * 255.0f);
        factor.r = brightness;
        factor.g = brightness;
        factor.b = brightness;
        factor.a = 255;
        J3DGXColor target{};
        target.r =
            static_cast<u8>(snapshot.isPupilMaterial ? 255.0f * snapshot.blend : 0.0f);
        target.g = 0;
        target.b = 0;
        target.a = 255;
        stage->setTevColorAB(GX_CC_CPREV, GX_CC_ZERO);
        stage->setTevColorCD(GX_CC_C0,
                             snapshot.isPupilMaterial ? GX_CC_KONST : GX_CC_ZERO);
        if (snapshot.isPupilMaterial) {
            tevBlock->setTevKColorSel(snapshot.stageIndex, GX_TEV_KCSEL_K0);
        }
        material->setTevColor(0, &factor);
        material->setTevKColor(0, &target);
    } else if (snapshot.isPupilMaterial &&
               (snapshot.hasTevKColor || snapshot.hasTevColor))
    {
        stage->setTevColorAB(GX_CC_ZERO, GX_CC_ZERO);
        if (snapshot.hasTevKColor) {
            stage->setTevColorCD(GX_CC_ZERO, GX_CC_KONST);
            tevBlock->setTevKColorSel(snapshot.stageIndex, GX_TEV_KCSEL_K0);
            J3DGXColor target{};
            target.r = 255;
            target.g = 0;
            target.b = 0;
            target.a = 255;
            material->setTevKColor(0, &target);
        } else {
            stage->setTevColorCD(GX_CC_ZERO, GX_CC_C0);
            J3DGXColorS10 target{};
            target.r = 255;
            target.g = 0;
            target.b = 0;
            target.a = 255;
            material->setTevColor(0, &target);
        }
    } else {
        stage->setTevColorAB(GX_CC_ZERO, GX_CC_ZERO);
        stage->setTevColorCD(GX_CC_ZERO, GX_CC_ZERO);
    }
    stage->setTevColorOp(
        GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}

HookAction apply_dark_link_material_before_display(ModContext*, void* args, void*, void*) {
    auto* material = mods::arg<J3DMaterial*>(args, 0);
    for (u16 i = 0; i < g_darkLinkDrawMaterialSnapshotCount; ++i) {
        DarkLinkDrawMaterialSnapshot& snapshot =
            g_darkLinkDrawMaterialSnapshots[i];
        if (snapshot.material == material) {
            apply_dark_link_material_output(snapshot);
            break;
        }
    }
    return HOOK_CONTINUE;
}

bool tint_goron_material_for_draw(daAlink_c* alink, J3DMaterial* material) {
    if (!g_goronTunicEquipped || alink == nullptr ||
        alink->getClothesChangeWaitTimer() != 0 || material == nullptr)
    {
        return false;
    }

    auto* bodyModel = alink->field_0x064C;
    auto* hatModel = alink->mpLinkHatModel != nullptr
                         ? alink->mpLinkHatModel->getModelData()
                         : nullptr;
    const u16* bodyIndices = nullptr;
    size_t bodyIndexCount = 0;
    const u16* hatIndices = nullptr;
    size_t hatIndexCount = 0;
    static constexpr u16 zoraBodyIndices[] = {13, 0, 1};
    static constexpr u16 magicBodyIndices[] = {11, 10, 9, 8, 6};
    static constexpr u16 casualBodyIndices[] = {7, 5};
    static constexpr u16 zoraHatIndices[] = {1};
    static constexpr u16 magicHatIndices[] = {2, 1};
    static constexpr u16 casualHatIndices[] = {0};

    if (alink->checkZoraWearAbility()) {
        bodyIndices = zoraBodyIndices;
        bodyIndexCount = std::size(zoraBodyIndices);
        hatIndices = zoraHatIndices;
        hatIndexCount = std::size(zoraHatIndices);
    } else if (alink->checkMagicArmorWearAbility()) {
        bodyIndices = magicBodyIndices;
        bodyIndexCount = std::size(magicBodyIndices);
        hatIndices = magicHatIndices;
        hatIndexCount = std::size(magicHatIndices);
    } else if (daAlink_c::checkCasualWearFlg()) {
        bodyIndices = casualBodyIndices;
        bodyIndexCount = std::size(casualBodyIndices);
        hatIndices = casualHatIndices;
        hatIndexCount = std::size(casualHatIndices);
    } else {
        return false;
    }

    auto matchesMaterial = [material](J3DModelData* model, const u16* indices,
                                      size_t count) {
        if (model == nullptr) {
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            const u16 index = indices[i];
            if (index < model->getMaterialNum() &&
                model->getMaterialNodePointer(index) == material)
            {
                tint_clothing_material(model, index);
                return true;
            }
        }
        return false;
    };

    return matchesMaterial(bodyModel, bodyIndices, bodyIndexCount) ||
           matchesMaterial(hatModel, hatIndices, hatIndexCount);
}

HookAction apply_dark_link_material_diff(ModContext*, void* args, void*, void*) {
    auto* material = mods::arg<J3DMaterial*>(args, 0);
    auto* alink = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    if (tint_goron_material_for_draw(alink, material)) {
        mods::arg_ref<u32>(args, 1) |= J3DDiffFlag_TevReg;
    }

    for (u16 i = 0; i < g_darkLinkDrawMaterialSnapshotCount; ++i) {
        DarkLinkDrawMaterialSnapshot& snapshot =
            g_darkLinkDrawMaterialSnapshots[i];
        if (snapshot.material == material) {
            apply_dark_link_material_output(snapshot);
            mods::arg_ref<u32>(args, 1) |=
                J3D_DIFF_TEVSTAGENUM(1) | J3DDiffFlag_TevReg;
            break;
        }
    }
    return HOOK_CONTINUE;
}

void set_dark_link_terminal_color(bool isPupilMaterial) {
    const GXColor color = {
        static_cast<u8>(isPupilMaterial ? 255 : 0), 0, 0, 255};
    GXSetNumTevStages(1);
    GXSetTevOrder(
        GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevColor(GX_TEVREG0, color);
    GXSetTevColorIn(
        GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C0);
    GXSetTevColorOp(
        GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(
        GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
    GXSetTevAlphaOp(
        GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}

HookAction apply_dark_link_terminal_output_before_shape_draw(
    ModContext*, void*, void*, void*) {
    J3DShapePacket* packet = j3dSys.getShapePacket();
    J3DModel* model = packet != nullptr ? packet->getModel() : nullptr;
    if (!g_darkLinkFormActive || g_darkLinkFormBlend < 1.0f)
    {
        return HOOK_CONTINUE;
    }

    J3DShape* shape = packet != nullptr ? packet->getShape() : nullptr;
    J3DMaterial* material = shape != nullptr ? shape->getMaterial() : nullptr;
    if (material == nullptr) {
        return HOOK_CONTINUE;
    }

    J3DModelData* modelData = model != nullptr ? model->getModelData() : nullptr;
    if (modelData == nullptr) {
        return HOOK_CONTINUE;
    }
    if (!is_dark_link_appearance_model_data(modelData)) {
        return HOOK_CONTINUE;
    }

    for (u16 i = 0; i < modelData->getMaterialNum(); ++i) {
        if (modelData->getMaterialNodePointer(i) != material) {
            continue;
        }

        auto* materialNames = modelData->getMaterialName();
        const char* name =
            materialNames != nullptr ? materialNames->getName(i) : nullptr;
        const bool isPupilMaterial =
            contains_case_insensitive(name, "pupil") ||
            contains_case_insensitive(name, "iris") ||
            contains_case_insensitive(name, "hitomi") ||
            material_uses_pupil_texture(modelData, material);
        set_dark_link_terminal_color(isPupilMaterial);
        break;
    }
    return HOOK_CONTINUE;
}

void double_dark_link_incoming_damage(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!g_darkLinkFormActive || alink == nullptr || alink->checkWolf()) {
        return;
    }

    *static_cast<f32*>(retval) *= 4.0f;
}

HookAction double_dark_link_sword_damage(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!g_darkLinkFormActive || alink == nullptr || alink->checkWolf()) {
        return HOOK_CONTINUE;
    }

    mods::arg_ref<int>(args, 4) *= 4;
    return HOOK_CONTINUE;
}

HookAction double_dark_link_cylinder_sword_damage(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    constexpr u32 kSwordAttackTypes = AT_TYPE_NORMAL_SWORD | AT_TYPE_MASTER_SWORD;
    if (!g_darkLinkFormActive || alink == nullptr || alink->checkWolf() ||
        (mods::arg<u32>(args, 1) & kSwordAttackTypes) == 0)
    {
        return HOOK_CONTINUE;
    }

    mods::arg_ref<int>(args, 5) *= 4;
    return HOOK_CONTINUE;
}

void restore_dark_link_model_after_draw(ModContext*, void*, void*, void*) {
    for (u16 i = 0; i < g_darkLinkDrawMaterialSnapshotCount; ++i) {
        DarkLinkDrawMaterialSnapshot& snapshot =
            g_darkLinkDrawMaterialSnapshots[i];
        if (snapshot.material == nullptr) {
            continue;
        }

        J3DTevBlock* tevBlock = snapshot.material->getTevBlock();
        if (tevBlock != nullptr) {
            tevBlock->setTevStage(snapshot.stageIndex, &snapshot.stage);
            if (snapshot.hasTevKColor) {
                tevBlock->setTevKColorSel(snapshot.stageIndex, snapshot.tevKColorSel);
            }
        }
        if (snapshot.hasTevColor) {
            snapshot.material->setTevColor(0, &snapshot.tevColor);
        }
        if (snapshot.hasTevKColor) {
            snapshot.material->setTevKColor(0, &snapshot.tevKColor);
        }
        snapshot.material = nullptr;
    }
    g_darkLinkDrawMaterialSnapshotCount = 0;
    g_darkLinkDrawModelData = nullptr;
}

void update_dark_link_form_blend() {
    const f32 step = 1.0f / static_cast<f32>(kDarkLinkTransitionFrames);
    if (g_darkLinkFormActive) {
        g_darkLinkFormBlend = std::min(1.0f, g_darkLinkFormBlend + step);
    } else {
        g_darkLinkFormBlend = std::max(0.0f, g_darkLinkFormBlend - step);
    }
}

HookAction handle_hover_boots_heavy_toggle(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || !g_hoverBootsTransitionPending ||
        alink->mProcID != daAlink_c::PROC_BOOTS_EQUIP)
    {
        return HOOK_CONTINUE;
    }

    g_hoverBootsTransitionPending = false;
    *static_cast<int*>(retval) = 0;
    return HOOK_SKIP_ORIGINAL;
}

void begin_hover_boots_toggle(daAlink_c* alink) {
    if (alink == nullptr) {
        return;
    }

    set_hover_boots_enabled(alink, !g_hoverBootsEquipped);
    g_hoverBootsTransitionPending = false;

    const bool playBootsAnimation =
        !alink->mLinkAcch.ChkGroundHit() ||
        (alink->mLinkAcch.ChkGroundHit() && !alink->checkModeFlg(0x70C52)) ||
        (alink->checkMagneBootsOn() && cBgW_CheckBGround(alink->mMagneBootsTopVec.y)) ||
        alink->mProcID == daAlink_c::PROC_HANG_CLIMB;
    if (playBootsAnimation) {
        g_hoverBootsTransitionPending = true;
        const int result = alink->procBootsEquipInit();
        if (result == 0 || alink->mProcID != daAlink_c::PROC_BOOTS_EQUIP) {
            g_hoverBootsTransitionPending = false;
        }
    }
}

void begin_secondary_hawk_grass_use(daAlink_c* alink, u8 button) {
    alink->mSelectItemId = button;
    if (g_hawkGrassArchiveLoaded) {
        begin_hawk_grass_whistle(alink, button);
        return;
    }

    g_pendingHawkGrassUse = alink;
    g_pendingHawkGrassButton = button;
    g_hawkGrassPhaseRequested = true;
    update_hawk_grass_resource();
}

bool can_start_sit_action(const daAlink_c* alink) {
    return alink != nullptr && !alink->checkWolf() && !alink->checkEventRun() &&
           (alink->mLinkAcch.ChkGroundHit() || is_hover_boots_hovering(alink)) &&
           (alink->mProcID == daAlink_c::PROC_WAIT ||
            alink->mProcID == daAlink_c::PROC_MOVE ||
            alink->mProcID == daAlink_c::PROC_ATN_ACTOR_WAIT ||
            alink->mProcID == daAlink_c::PROC_ATN_ACTOR_MOVE);
}

void align_link_to_ground(daAlink_c* alink) {
    const f32 groundHeight = alink->mLinkAcch.GetGroundH();
    if (groundHeight == -G_CM3D_F_INF) {
        return;
    }

    alink->current.pos.y = groundHeight;
    if (!dComIfG_Bgsp().ChkPolySafe(alink->mLinkAcch.m_gnd)) {
        return;
    }

    cM3dGPla groundPlane;
    if (!dComIfG_Bgsp().GetTriPla(alink->mLinkAcch.m_gnd, &groundPlane) ||
        !cBgW_CheckBGround(groundPlane.mNormal.y))
    {
        return;
    }

    alink->shape_angle.x =
        fopAcM_getPolygonAngle(&groundPlane, alink->shape_angle.y);
    alink->shape_angle.z =
        -fopAcM_getPolygonAngle(&groundPlane, alink->shape_angle.y + 0x4000);
}

void restore_sit_shape_angles(daAlink_c* alink) {
    alink->shape_angle.x = g_sitOriginalShapeAngleX;
    alink->shape_angle.z = g_sitOriginalShapeAngleZ;
}

void begin_sit_action(daAlink_c* alink) {
    if (!can_start_sit_action(alink)) {
        return;
    }

    const bool hovering = is_hover_boots_hovering(alink);
    g_sitOriginalShapeAngleX = alink->shape_angle.x;
    g_sitOriginalShapeAngleZ = alink->shape_angle.z;
    alink->procWaitInit();
    alink->mNormalSpeed = 0.0f;
    alink->speedF = 0.0f;
    if (hovering) {
        alink->current.pos.y = g_hoverGroundHeight;
        alink->speed.y = 0.0f;
        alink->mSinkShapeOffset = 0.0f;
    } else {
        align_link_to_ground(alink);
    }
    alink->setSingleAnimeBase(daAlink_c::ANM_WAIT_SIT);
    g_sitActionState = SitActionState::Seated;
}

void begin_stand_action(daAlink_c* alink) {
    if (alink == nullptr || g_sitActionState != SitActionState::Seated) {
        return;
    }

    restore_sit_shape_angles(alink);
    alink->mSinkShapeOffset = 0.0f;
    alink->setSingleAnimeBase(daAlink_c::ANM_WAIT);
    g_sitActionState = SitActionState::Inactive;
}

HookAction preserve_sit_idle_animation(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (g_sitActionState == SitActionState::Inactive || alink == nullptr ||
        alink->mProcID != daAlink_c::PROC_WAIT)
    {
        return HOOK_CONTINUE;
    }

    *static_cast<int*>(retval) = 1;
    return HOOK_SKIP_ORIGINAL;
}

void suppress_sit_stick_input(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (g_sitActionState == SitActionState::Inactive || alink == nullptr) {
        return;
    }

    alink->mStickValue = 0.0f;
    alink->mMoveValue = 0.0f;
    alink->mNormalSpeed = 0.0f;
    alink->speedF = 0.0f;
}

void update_sit_action() {
    if (g_sitActionState == SitActionState::Inactive) {
        return;
    }

    auto* alink = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    const bool grounded = alink != nullptr && alink->mLinkAcch.ChkGroundHit();
    const bool hovering = is_hover_boots_hovering(alink);
    if (alink == nullptr || alink->checkWolf() || alink->checkEventRun() ||
        alink->mProcID != daAlink_c::PROC_WAIT || (!grounded && !hovering))
    {
        if (alink != nullptr) {
            restore_sit_shape_angles(alink);
            alink->mSinkShapeOffset = 0.0f;
        }
        g_sitActionState = SitActionState::Inactive;
        return;
    }

    if (hovering) {
        alink->current.pos.y = g_hoverGroundHeight;
        alink->speed.y = 0.0f;
        alink->mSinkShapeOffset = 0.0f;
        alink->mNormalSpeed = 0.0f;
        alink->speedF = 0.0f;
        return;
    }

    if (alink->mLinkAcch.GetGroundH() == -G_CM3D_F_INF) {
        restore_sit_shape_angles(alink);
        alink->mSinkShapeOffset = 0.0f;
        g_sitActionState = SitActionState::Inactive;
        return;
    }

    align_link_to_ground(alink);
    alink->mNormalSpeed = 0.0f;
    alink->speedF = 0.0f;
}

void save_and_disable_sit_music_modifiers(Z2SeqMgr* sequenceManager) {
    if (!g_sitMusicModifiersSaved) {
        g_sitMusicPreviousHeightVolMod = sequenceManager->mFlags.mHeightVolMod;
        g_sitMusicPreviousTimeProcVolMod = sequenceManager->mFlags.mTimeProcVolMod;
        g_sitMusicModifiersSaved = true;
    }

    sequenceManager->setHeightVolMod(false, 0);
    sequenceManager->setTimeProcVolMod(false, 0);
    sequenceManager->field_0xa4.forceIn();
}

void restore_sit_music_modifiers(Z2SeqMgr* sequenceManager) {
    if (!g_sitMusicModifiersSaved || sequenceManager == nullptr) {
        return;
    }

    sequenceManager->setHeightVolMod(g_sitMusicPreviousHeightVolMod, 30);
    sequenceManager->setTimeProcVolMod(g_sitMusicPreviousTimeProcVolMod, 0);
    g_sitMusicModifiersSaved = false;
}

u32 expected_sit_music_bgm_id(u32 selectedTrack) {
    return selectedTrack == Z2BGM_LUTERA_DEMO ? Z2BGM_LUTERA2 : selectedTrack;
}

const SitMusicWaveMapping* sit_music_wave_mapping(u32 track) {
    for (const auto& mapping : kSitMusicWaveMappings) {
        if (mapping.track == track) {
            return &mapping;
        }
    }
    return nullptr;
}

bool sit_music_mapping_uses_wave(const SitMusicWaveMapping* mapping, u32 wave) {
    return mapping != nullptr &&
        (mapping->wave1 == wave || mapping->alternateWave1 == wave ||
         mapping->wave2 == wave);
}

void report_sit_music_wave_failure(u32 track, u32 wave, const char* action) {
    if (g_sitMusicWaveFailureTrack == track) {
        return;
    }

    char message[128];
    std::snprintf(message, sizeof(message),
                  "Sit music wave %02x for track %08x could not %s",
                  wave, track, action);
    svc_log->warn(mod_ctx, message);
    g_sitMusicWaveFailureTrack = track;
}

bool prepare_sit_music_waves(u32 track) {
    const SitMusicWaveMapping* mapping = sit_music_wave_mapping(track);
    if (mapping == nullptr) {
        return true;
    }

    if (g_sitMusicWaveFailureTrack == track) {
        return false;
    }

    auto* sceneManager = Z2GetSceneMgr();
    if (sceneManager == nullptr) {
        return false;
    }

    const u8 waves[] = {mapping->wave1, mapping->alternateWave1, mapping->wave2};
    for (const u8 wave : waves) {
        if (wave == 0) {
            continue;
        }

        const s32 status = sceneManager->getBgmLoadStatus(wave);
        if (status == 2) {
            g_sitMusicWaveReleaseFrames[wave] = 0;
            g_sitMusicWaveReleaseScheduled[wave] = false;
            continue;
        }
        if (status == 1) {
            return false;
        }

        if (!sceneManager->loadBgmWave(wave)) {
            report_sit_music_wave_failure(track, wave, "load");
            return false;
        }
        g_sitMusicOwnedWaves[wave] = true;
        g_sitMusicWaveReleaseFrames[wave] = 0;
        g_sitMusicWaveReleaseScheduled[wave] = false;
        return false;
    }

    return true;
}

void schedule_sit_music_wave_releases(u32 activeTrack, u8 delayFrames) {
    const SitMusicWaveMapping* mapping = sit_music_wave_mapping(activeTrack);
    for (u32 wave = 1; wave < g_sitMusicOwnedWaves.size(); ++wave) {
        if (g_sitMusicOwnedWaves[wave] &&
            !sit_music_mapping_uses_wave(mapping, wave))
        {
            g_sitMusicWaveReleaseFrames[wave] = delayFrames;
            g_sitMusicWaveReleaseScheduled[wave] = true;
        }
    }
}

void update_sit_music_wave_cleanup() {
    auto* sceneManager = Z2GetSceneMgr();
    if (sceneManager == nullptr) {
        return;
    }

    const int64_t selection = g_sitActionState == SitActionState::Seated
        ? sitMusicSelection()
        : 0;
    const u32 selectedTrack = selection > 0
        ? expected_sit_music_bgm_id(
              kFirstSitMusicBgmId + static_cast<u32>(selection - 1))
        : 0;
    const SitMusicWaveMapping* selectedMapping = sit_music_wave_mapping(selectedTrack);

    for (u32 wave = 1; wave < g_sitMusicOwnedWaves.size(); ++wave) {
        if (!g_sitMusicOwnedWaves[wave]) {
            continue;
        }
        if (sit_music_mapping_uses_wave(selectedMapping, wave)) {
            g_sitMusicWaveReleaseFrames[wave] = 0;
            g_sitMusicWaveReleaseScheduled[wave] = false;
            continue;
        }
        if (wave == sceneManager->loadedBgmWave_1 ||
            wave == sceneManager->loadedBgmWave_2)
        {
            g_sitMusicOwnedWaves[wave] = false;
            g_sitMusicWaveReleaseFrames[wave] = 0;
            g_sitMusicWaveReleaseScheduled[wave] = false;
            continue;
        }
        if (!g_sitMusicWaveReleaseScheduled[wave]) {
            g_sitMusicWaveReleaseFrames[wave] = 30;
            g_sitMusicWaveReleaseScheduled[wave] = true;
        }
        if (g_sitMusicWaveReleaseFrames[wave] > 0) {
            --g_sitMusicWaveReleaseFrames[wave];
            continue;
        }

        const s32 status = sceneManager->getBgmLoadStatus(wave);
        if (status == 0) {
            g_sitMusicOwnedWaves[wave] = false;
            g_sitMusicWaveReleaseScheduled[wave] = false;
            continue;
        }
        if (status == 1) {
            continue;
        }
        if (!sceneManager->eraseBgmWave(wave)) {
            if (!g_sitMusicWaveEraseFailureReported[wave]) {
                char message[96];
                std::snprintf(message, sizeof(message),
                              "Failed to release Sit music wave %02x", wave);
                svc_log->warn(mod_ctx, message);
                g_sitMusicWaveEraseFailureReported[wave] = true;
            }
            continue;
        }
        g_sitMusicOwnedWaves[wave] = false;
        g_sitMusicWaveEraseFailureReported[wave] = false;
        g_sitMusicWaveReleaseScheduled[wave] = false;
    }
}

bool start_sit_music_track(u32 selectedTrack) {
    auto* sequenceManager = Z2GetSeqMgr();
    auto* audioManager = Z2GetAudioMgr();
    if (sequenceManager == nullptr || audioManager == nullptr) {
        return false;
    }

    const u32 track = expected_sit_music_bgm_id(selectedTrack);
    if (!prepare_sit_music_waves(track)) {
        return false;
    }

    if (track == Z2BGM_LAKE || track == Z2BGM_GAME_OVER) {
        auto* soundManager = Z2GetSoundMgr();
        if (soundManager == nullptr) {
            return false;
        }

        save_and_disable_sit_music_modifiers(sequenceManager);
        audioManager->unMuteSceneBgm(0);
        sequenceManager->bgmStop(0, 0);
        if (!soundManager->startSound(track, sequenceManager->getMainBgmHandle(), nullptr)) {
            restore_sit_music_modifiers(sequenceManager);
            return false;
        }
        sequenceManager->mBgmStatus = 0xff;
        sequenceManager->mMainBgmMaster.fadeInFromOut(30);
        schedule_sit_music_wave_releases(track, 30);
        return true;
    }

    save_and_disable_sit_music_modifiers(sequenceManager);
    audioManager->unMuteSceneBgm(0);
    audioManager->bgmStart(track, 30, 0);
    schedule_sit_music_wave_releases(track, 30);
    return true;
}

void log_sit_music_start_failure(u32 selectedTrack, u32 activeTrack) {
    if (g_sitMusicReportedFailureTrack == selectedTrack) {
        return;
    }

    const size_t trackIndex = selectedTrack - kFirstSitMusicBgmId;
    const char* trackName = trackIndex < kSitMusicTrackCount
        ? kSitMusicTrackNames[trackIndex] : "UNKNOWN";
    char message[128];
    std::snprintf(message, sizeof(message),
                  "Sit music %s (%08x) did not become the main sequence; active ID is %08x",
                  trackName, selectedTrack, activeTrack);
    svc_log->warn(mod_ctx, message);
    g_sitMusicReportedFailureTrack = selectedTrack;
}

void restore_sit_area_music() {
    if (!g_sitMusicOverrideActive) {
        auto* sequenceManager = Z2GetSeqMgr();
        restore_sit_music_modifiers(sequenceManager);
        g_sitMusicWaveFailureTrack = 0;
        return;
    }

    auto* sceneManager = Z2GetAudioMgr();
    if (sceneManager == nullptr) {
        return;
    }

    auto* sequenceManager = Z2GetSeqMgr();
    if (sequenceManager == nullptr) {
        return;
    }

    restore_sit_music_modifiers(sequenceManager);
    sceneManager->sceneBgmStart();
    schedule_sit_music_wave_releases(0, 30);
    g_sitMusicOverrideActive = false;
    g_sitMusicActiveTrack = 0;
    g_sitMusicReportedFailureTrack = 0;
    g_sitMusicWaveFailureTrack = 0;
}

void update_sit_music() {
    update_sit_music_wave_cleanup();
    if (g_sitActionState != SitActionState::Seated) {
        restore_sit_area_music();
        return;
    }

    const int64_t selection = sitMusicSelection();
    if (selection == 0) {
        restore_sit_area_music();
        return;
    }

    auto* sequenceManager = Z2GetSeqMgr();
    if (sequenceManager == nullptr || Z2GetAudioMgr() == nullptr) {
        return;
    }

    const u32 track = kFirstSitMusicBgmId + static_cast<u32>(selection - 1);
    if (g_sitMusicWaveFailureTrack != track) {
        g_sitMusicWaveFailureTrack = 0;
    }
    if (g_sitMusicOverrideActive && g_sitMusicActiveTrack == track) {
        return;
    }

    if (!start_sit_music_track(track)) {
        return;
    }
    g_sitMusicOverrideActive = true;
    g_sitMusicActiveTrack = track;
    if (sequenceManager->getMainBgmID() == expected_sit_music_bgm_id(track)) {
        g_sitMusicReportedFailureTrack = 0;
    } else {
        log_sit_music_start_failure(track, sequenceManager->getMainBgmID());
    }
}

void enforce_sit_music_after_audio_framework(ModContext*, void* args, void*, void*) {
    const int64_t selection = sitMusicSelection();
    if (g_sitActionState != SitActionState::Seated || selection == 0) {
        return;
    }

    auto* sequenceManager = mods::arg<Z2SeqMgr*>(args, 0);
    if (sequenceManager == nullptr) {
        return;
    }

    const u32 track = kFirstSitMusicBgmId + static_cast<u32>(selection - 1);
    if (sequenceManager->getMainBgmID() == expected_sit_music_bgm_id(track)) {
        g_sitMusicOverrideActive = true;
        g_sitMusicActiveTrack = track;
        g_sitMusicReportedFailureTrack = 0;
        return;
    }

    if (g_sitMusicWaveFailureTrack != track) {
        g_sitMusicWaveFailureTrack = 0;
    }
    if (!start_sit_music_track(track)) {
        return;
    }
    g_sitMusicOverrideActive = true;
    g_sitMusicActiveTrack = track;
    if (sequenceManager->getMainBgmID() == expected_sit_music_bgm_id(track)) {
        g_sitMusicReportedFailureTrack = 0;
    } else {
        log_sit_music_start_failure(track, sequenceManager->getMainBgmID());
    }
}

void request_sit_action(daAlink_c* alink) {
    begin_sit_action(alink);
}

void update_secondary_assigned_item_input(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr) {
        return;
    }

    const bool xTriggered = mDoCPd_c::getTrigX(PAD_1) != 0;
    const bool yTriggered = mDoCPd_c::getTrigY(PAD_1) != 0;
    if (!xTriggered && !yTriggered) {
        return;
    }

    if (g_secondaryRing != nullptr || g_primaryRingOpen || alink->checkEventRun()) {
        g_secondaryItemInputHandled = true;
        return;
    }

    const u8 pressedButtons = static_cast<u8>(
        (secondaryItemAssigned(0) && xTriggered
             ? daAlink_c::BTN_X : 0) |
        (secondaryItemAssigned(1) && yTriggered
             ? daAlink_c::BTN_Y : 0));
    if (pressedButtons == 0) {
        return;
    }

    alink->mItemTrigger &= static_cast<u8>(~pressedButtons);
    if (g_secondaryItemInputHandled) {
        return;
    }

    const int button = (pressedButtons & daAlink_c::BTN_X) != 0 ? 0 : 1;
    const u8 itemId = secondaryItemIdForSlot(g_secondarySelectedItems[button]);
    g_secondaryItemInputHandled = true;
    switch (itemId) {
    case kSitItemId:
        if (g_sitActionState == SitActionState::Inactive) {
            request_sit_action(alink);
        } else if (g_sitActionState == SitActionState::Seated) {
            begin_stand_action(alink);
        }
        break;
    case kGoronTunicItemId:
        g_goronTunicEquipped = !g_goronTunicEquipped;
        g_goronLavaContactFrames = 0;
        svc_log->info(mod_ctx, g_goronTunicEquipped ? "Goron Tunic equipped" :
                                                     "Goron Tunic unequipped");
        break;
    case kHawkGrassItem:
        begin_secondary_hawk_grass_use(alink, static_cast<u8>(button));
        break;
    case kHoverBootsItem:
        begin_hover_boots_toggle(alink);
        break;
    case kDarkLinkItem:
        g_darkLinkFormActive = !g_darkLinkFormActive;
        play_dark_link_transition_effect(alink, g_darkLinkFormActive);
        svc_log->info(mod_ctx, g_darkLinkFormActive ? "Dark Link Form activated" :
                                                     "Dark Link Form deactivated");
        break;
    default:
        break;
    }
}

void tint_goron_tunic_hud_icon(ModContext*, void* args, void*, void*) {
    if (g_secondaryIconContext != SecondaryIconContext::GoronTunic) {
        return;
    }

    const int button = mods::arg<int>(args, 1);
    if (button < 0 || button > 1) {
        return;
    }
    auto* meter = mods::arg<dMeter2Draw_c*>(args, 0);
    tint_tunic_icon(meter->mpItemXY[button]);
    tint_tunic_icon(meter->mpItemXYPane[button]);
}

ModResult build_panel(ModContext*, UiElementHandle panel, void*, ModError*) {
    static const char* kWheelDirectionOptions[] = {"Left", "Down"};
    initialize_sit_music_options();
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_DROPDOWN;
    control.label = "D-pad direction that opens the secondary inventory wheel";
    control.help_rml =
        "The other Left/Down direction opens the map. In the secondary wheel, press X or Y on "
        "the Goron Tunic or Hover Boots to equip or unequip it. Hover Boots hold Link at the "
        "height where he left the ground and emit a ripple while airborne. Optional water "
        "walking is available in this settings panel. Secondary X/Y assignments are private "
        "and take precedence over vanilla items on that button. The tunic color override is applied after "
        "game-level model color updates; integration with Crests' remote-player color "
        "replication is experimental.";
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = g_wheelOpenDirection;
    control.options = kWheelDirectionOptions;
    control.option_count = 2;
    ModResult result = svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
    if (result != MOD_OK) {
        return result;
    }

    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_DROPDOWN;
    control.label = "Music while sitting";
    control.help_rml =
        "Choose a game music track to play while Link is seated. Default (Area Music) leaves "
        "the intended area soundtrack playing. The area soundtrack resumes when Link stands "
        "or the Sit action ends.";
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = g_sitMusicTrack;
    control.options = g_sitMusicOptionLabels.data();
    control.option_count = g_sitMusicOptionLabels.size();
    result = svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
    if (result != MOD_OK) {
        return result;
    }

    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE;
    control.label = "Walk on lava indefinitely";
    control.help_rml =
        "When the Goron Tunic is equipped, stay at the lava surface without sinking or "
        "triggering the five-second lava return.";
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = g_infiniteLavaWalk;
    result = svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
    if (result != MOD_OK) {
        return result;
    }

    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE;
    control.label = "Twilight Ward (Magic Armor)";
    control.help_rml =
        "When Magic Armor is worn, hits still deal damage but do not interrupt Link, stun him, "
        "or knock him back.";
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = g_twilightWard;
    result = svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
    if (result != MOD_OK) {
        return result;
    }

    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE;
    control.label = "Hawk Grass one-hit attacks";
    control.help_rml =
        "The Hawk called by the secondary Hawk Grass deals one-hit damage with its attacks.";
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = g_hawkOneShotDamage;
    result = svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
    if (result != MOD_OK) {
        return result;
    }

    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE;
    control.label = "Hover Boots water walking (replaces airwalking)";
    control.help_rml =
        "When enabled, Hover Boots walk on water and no longer airwalk. Disabled by default.";
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = g_hoverBootsWaterWalking;
    result = svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
    if (result != MOD_OK) {
        return result;
    }

    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE;
    control.label = "Rupee Magnet";
    control.help_rml = "Collect nearby rupees automatically. The radius is configurable below.";
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = g_rupeeMagnetEnabled;
    result = svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
    if (result != MOD_OK) {
        return result;
    }

    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_NUMBER;
    control.label = "Rupee Magnet radius";
    control.help_rml =
        "100% is 300 game units, matching the game's existing rupee-return distance. "
        "The configurable range is 50% (150 units) to 300% (900 units).";
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = g_rupeeMagnetRadiusPercent;
    control.min = kRupeeMagnetMinRadiusPercent;
    control.max = kRupeeMagnetMaxRadiusPercent;
    control.step = 25;
    control.suffix = "%";
    return svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
}

ModResult build_robin_settings_tab(
    ModContext* ctx, UiWindowHandle, UiElementHandle left_pane, UiElementHandle, void* user_data,
    ModError* error) {
    return build_panel(ctx, left_pane, user_data, error);
}

void on_robin_settings_window_closed(ModContext*, UiWindowHandle, void*) {
    g_robinSettingsWindow = 0;
}

void open_robin_settings_window(ModContext* ctx, void*) {
    if (g_robinSettingsWindow != 0) {
        return;
    }

    UiTabDesc tabs[] = {UI_TAB_DESC_INIT};
    tabs[0].title = "Settings";
    tabs[0].build = build_robin_settings_tab;

    UiWindowDesc desc = UI_WINDOW_DESC_INIT;
    desc.tabs = tabs;
    desc.tab_count = 1;
    desc.on_closed = on_robin_settings_window_closed;
    const ModResult result = svc_ui->window_push(ctx, &desc, &g_robinSettingsWindow);
    if (result != MOD_OK) {
        svc_log->error(ctx, "failed to open Robin's W&T settings");
    }
}

#define ADD_PRE(HookType, callback)                                                                \
    do {                                                                                           \
        const ModResult hookResult = mods::hook::add_pre<HookType>(callback);                      \
        if (hookResult != MOD_OK) {                                                                \
            svc_log->error(mod_ctx, "failed to install secondary wheel hook");                   \
            return hookResult;                                                                     \
        }                                                                                          \
    } while (false)

#define ADD_POST(HookType, callback)                                                               \
    do {                                                                                           \
        const ModResult hookResult = mods::hook::add_post<HookType>(callback);                     \
        if (hookResult != MOD_OK) {                                                                \
            svc_log->error(mod_ctx, "failed to install secondary wheel hook");                   \
            return hookResult;                                                                     \
        }                                                                                          \
    } while (false)

DEFINE_HOOK(&dMenu_Ring_c::openExplain, OpenExplain);
DEFINE_HOOK(&dMenu_Ring_c::setNameString, SetNameString);
DEFINE_HOOK(&dMenu_Ring_c::setSelectItem, RingSetSelectItem);
DEFINE_HOOK(&dMenu_Ring_c::setJumpItem, RingSetJumpItem);
DEFINE_HOOK(&dMeter2Draw_c::changeTextureItemXY, ChangeTextureItemXY);
DEFINE_HOOK(&dMeter2_c::moveButtonXY, MeterMoveButtonXY);
DEFINE_HOOK(&dMeter2Info_readItemTexture, ReadItemTexture);
DEFINE_HOOK(&daAlink_c::setWaterY, SetWaterY);
DEFINE_HOOK(&daAlink_c::setRoomInfo, SetRoomInfo);
DEFINE_HOOK(&daAlink_c::setStickData, SetStickData);
DEFINE_HOOK(&daAlink_c::checkWaitAction, CheckWaitAction);
DEFINE_HOOK(&daAlink_c::procDamageInit, ProcDamageInit);
DEFINE_HOOK(&daAlink_c::procCoLargeDamageInit, ProcCoLargeDamageInit);
DEFINE_HOOK(&daAlink_c::procLargeDamageUpInit, ProcLargeDamageUpInit);
DEFINE_HOOK(&daAlink_c::procCoLargeDamageWallInit, ProcCoLargeDamageWallInit);
DEFINE_HOOK(&daAlink_c::procCoPolyDamageInit, ProcCoPolyDamageInit);
DEFINE_HOOK(&daAlink_c::procCoElecDamageInit, ProcCoElecDamageInit);
DEFINE_HOOK(&daAlink_c::procGuardBreakInit, ProcGuardBreakInit);
DEFINE_HOOK(&daAlink_c::procHorseDamageInit, ProcHorseDamageInit);
DEFINE_HOOK(&daAlink_c::procSwimDamageInit, ProcSwimDamageInit);
DEFINE_HOOK(&daAlink_c::autoGroundHit, AutoGroundHit);
DEFINE_HOOK(&daAlink_c::getGroundAngle, GetGroundAngle);
DEFINE_HOOK(&daAlink_c::getSlidePolygon, GetSlidePolygon);
DEFINE_HOOK(&daAlink_c::checkSlope, CheckSlope);
DEFINE_HOOK(&daAlink_c::setStepsOffset, SetStepsOffset);
DEFINE_HOOK(&daAlink_c::iceSlipBgCheck, IceSlipBgCheck);
DEFINE_HOOK(&daAlink_c::swimOutAfter, SwimOutAfter);
DEFINE_HOOK(&daAlink_c::checkAutoJumpAction, CheckAutoJumpAction);
DEFINE_HOOK(&daAlink_c::procLand, ProcLand);
DEFINE_HOOK(&daAlink_c::procHangStartInit, ProcHangStartInit);
DEFINE_HOOK(&daAlink_c::procHangFallStartInit, ProcHangFallStartInit);
DEFINE_HOOK(&daAlink_c::checkLandAction, CheckLandAction);
DEFINE_HOOK(&daAlink_c::setSandShapeOffset, SetSandShapeOffset);
DEFINE_HOOK(&daAlink_c::checkWaterPolygonUnder, CheckWaterPolygonUnder);
DEFINE_HOOK(&daAlink_c::checkPolyDamage, CheckPolyDamage);
DEFINE_HOOK(&daAlink_c::setHeavyBoots, SetHeavyBoots);
DEFINE_HOOK(&daAlink_c::voiceStart, HawkGrassVoiceStart);
DEFINE_HOOK(&dMenu_Ring_c::_move, RingMove);
DEFINE_HOOK(&dMenu_Ring_c::_draw, RingDraw);
DEFINE_HOOK(&dMenu_Ring_c::isOpen, RingIsOpen);
DEFINE_HOOK(&dMenu_Ring_c::isMoveEnd, RingIsMoveEnd);
DEFINE_HOOK(&dMenu_Ring_c::isClose, RingIsClose);
DEFINE_HOOK(&dMw_c::key_wait_proc, KeyWait);
DEFINE_HOOK(&dMw_c::dMw_ring_create, RingCreate);
DEFINE_HOOK(&dMw_c::dMw_ring_delete, RingDelete);
DEFINE_HOOK(&dMw_DOWN_TRIGGER, DownTrigger);
DEFINE_HOOK(&dMw_LEFT_TRIGGER, LeftTrigger);
DEFINE_HOOK(&dMeterMap_c::meter_map_move, MapMeterMove);
DEFINE_HOOK(&dMeterMap_c::keyCheck, MapKeyCheck);
DEFINE_HOOK(&dMeterMap_c::ctrlShowMap, MapShow);
DEFINE_HOOK(&daAlink_c::checkRestartRoom, CheckRestartRoom);
DEFINE_HOOK(&daAlink_c::procCoLavaReturnInit, ProcCoLavaReturnInit);
DEFINE_HOOK(&daAlink_c::checkSceneChange, CheckSceneChange);
DEFINE_HOOK(&daAlink_c::modelCalc, ModelCalc);
DEFINE_HOOK(&Z2SeqMgr::processBgmFramework, ProcessBgmFramework);
DEFINE_HOOK(&daAlink_c::modelDraw, ModelDraw);
DEFINE_HOOK(&J3DMaterial::makeDisplayList, MaterialMakeDisplayList);
DEFINE_HOOK(&J3DMaterial::diff, MaterialDiff);
DEFINE_HOOK(&J3DShapeDraw::draw, ShapeDraw);
DEFINE_HOOK(&daAlink_c::damageMagnification, DamageMagnification);
DEFINE_HOOK(&daAlink_c::setSwordAtParam, SwordAtParam);
DEFINE_HOOK(&daAlink_c::setCylAtParam, CylinderAtParam);
DEFINE_HOOK(static_cast<void (dScnKy_env_light_c::*)(J3DModelData*, dKy_tevstr_c*)>(
                &dScnKy_env_light_c::setLightTevColorType_MAJI),
            LightTevColorTypeMaji);
DEFINE_HOOK(&daAlink_c::setWaterDropColor, SetWaterDropColor);
DEFINE_HOOK(&daItem_c::_daItem_execute, ItemExecute);

ModResult install_hooks() {
    ADD_PRE(OpenExplain, suppress_secondary_explain);
    ADD_POST(SetNameString, name_secondary_item);
    ADD_PRE(RingSetSelectItem, prepare_secondary_equip_icon);
    ADD_POST(RingSetSelectItem, finish_secondary_equip_icon);
    ADD_POST(RingSetJumpItem, clear_secondary_assignment_for_vanilla_ring);
    ADD_PRE(ChangeTextureItemXY, prepare_hover_boots_hud_icon);
    ADD_POST(ChangeTextureItemXY, tint_goron_tunic_hud_icon);
    ADD_POST(ChangeTextureItemXY, finish_hover_boots_hud_icon);
    ADD_POST(MeterMoveButtonXY, refresh_secondary_hud_assignments);
    ADD_PRE(ReadItemTexture, apply_hawk_grass_equip_icon);
    ADD_POST(ReadItemTexture, replace_secondary_custom_icon);
    ADD_POST(SetWaterY, treat_lava_as_surface);
    ADD_POST(SetWaterY, treat_water_as_surface_with_hover_boots);
    ADD_POST(SetRoomInfo, maintain_lava_ground);
    ADD_POST(SetRoomInfo, maintain_hover_water_surface);
    ADD_POST(AutoGroundHit, maintain_hover_height);
    ADD_PRE(GetGroundAngle, flatten_hover_water_ground_angle);
    ADD_PRE(GetSlidePolygon, suppress_submerged_water_slide_polygon);
    ADD_PRE(CheckSlope, suppress_submerged_water_slope);
    ADD_PRE(SetStepsOffset, suppress_submerged_water_step_offset);
    ADD_PRE(IceSlipBgCheck, skip_submerged_ice_slip);
    ADD_PRE(SwimOutAfter, keep_hover_boots_on_water_surface);
    ADD_PRE(CheckAutoJumpAction, handle_hover_airborne_action);
    ADD_POST(ProcLand, continue_hover_landing);
    ADD_PRE(ProcHangStartInit, suppress_hover_ledge_grab);
    ADD_PRE(ProcHangFallStartInit, suppress_hover_ledge_grab);
    ADD_PRE(CheckLandAction, land_on_lava_surface);
    ADD_PRE(SetSandShapeOffset, lower_sit_model_to_ground);
    ADD_PRE(SetSandShapeOffset, prepare_lava_sinking);
    ADD_PRE(CheckWaterPolygonUnder, suppress_lava_submersion);
    ADD_PRE(CheckPolyDamage, suppress_lava_damage);
    ADD_PRE(ProcDamageInit, suppress_twilight_ward_hit_stun);
    ADD_PRE(ProcCoLargeDamageInit, suppress_twilight_ward_hit_stun);
    ADD_PRE(ProcLargeDamageUpInit, suppress_twilight_ward_hit_stun);
    ADD_PRE(ProcCoLargeDamageWallInit, suppress_twilight_ward_hit_stun);
    ADD_PRE(ProcCoPolyDamageInit, suppress_twilight_ward_hit_stun);
    ADD_PRE(ProcCoElecDamageInit, suppress_twilight_ward_hit_stun);
    ADD_PRE(ProcGuardBreakInit, suppress_twilight_ward_hit_stun);
    ADD_PRE(ProcHorseDamageInit, suppress_twilight_ward_hit_stun);
    ADD_PRE(ProcSwimDamageInit, suppress_twilight_ward_hit_stun);
    ADD_PRE(SetHeavyBoots, handle_hover_boots_heavy_toggle);
    ADD_POST(SetStickData, update_secondary_assigned_item_input);
    ADD_POST(SetStickData, suppress_sit_stick_input);
    ADD_PRE(CheckWaitAction, preserve_sit_idle_animation);
    ADD_POST(HawkGrassVoiceStart, spawn_hawk_after_grass_sound);
    ADD_PRE(RingMove, handle_secondary_ring_move);
    ADD_PRE(RingDraw, handle_secondary_ring_draw);
    ADD_PRE(RingIsMoveEnd, handle_secondary_ring_move_end);
    ADD_PRE(RingIsOpen, open_secondary_ring);
    ADD_PRE(RingIsClose, close_secondary_ring);

    ADD_PRE(KeyWait, begin_key_wait);
    ADD_POST(KeyWait, end_key_wait);
    ADD_PRE(RingCreate, prepare_secondary_wheel);
    ADD_POST(RingCreate, finish_secondary_wheel);
    ADD_PRE(RingDelete, prepare_ring_delete);
    ADD_POST(RingDelete, clear_secondary_wheel);
    ADD_PRE(DownTrigger, remap_down_trigger);
    ADD_PRE(LeftTrigger, remap_left_trigger);

    ADD_PRE(MapMeterMove, begin_map_input);
    ADD_POST(MapMeterMove, end_map_input);
    ADD_PRE(MapKeyCheck, begin_map_input);
    ADD_POST(MapKeyCheck, end_map_input);
    ADD_PRE(MapShow, begin_map_input);
    ADD_POST(MapShow, end_map_input);
    ADD_PRE(CheckRestartRoom, prevent_lava_return);
    ADD_PRE(ProcCoLavaReturnInit, delay_goron_lava_return);
    ADD_PRE(CheckRestartRoom, suppress_water_surface_restart);
    ADD_PRE(CheckSceneChange, suppress_submerged_water_exit);
    ADD_POST(ModelCalc, reapply_tunic_override_after_model_calc);
    ADD_PRE(ModelCalc, offset_sit_model_forward_before_calc);
    ADD_POST(ProcessBgmFramework, enforce_sit_music_after_audio_framework);
    ADD_PRE(ModelDraw, mark_dark_link_model_for_draw);
    ADD_POST(LightTevColorTypeMaji, reapply_tunic_override_after_lighting);
    ADD_POST(LightTevColorTypeMaji, apply_dark_link_after_lighting);
    ADD_PRE(MaterialMakeDisplayList, apply_dark_link_material_before_display);
    ADD_PRE(MaterialDiff, apply_dark_link_material_diff);
    ADD_PRE(ShapeDraw, apply_dark_link_terminal_output_before_shape_draw);
    ADD_POST(DamageMagnification, double_dark_link_incoming_damage);
    ADD_PRE(SwordAtParam, double_dark_link_sword_damage);
    ADD_PRE(CylinderAtParam, double_dark_link_cylinder_sword_damage);
    ADD_POST(ModelDraw, restore_dark_link_model_after_draw);
    ADD_POST(ModelCalc, update_hover_magic_after_model_calc);
    ADD_POST(SetWaterDropColor, tint_goron_tunic);
    ADD_POST(ItemExecute, collect_nearby_rupees);
    return MOD_OK;
}

#undef ADD_PRE
#undef ADD_POST

}  // namespace

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError* error) {
    g_darkLinkFormActive = false;
    g_darkLinkFormBlend = 0.0f;
    g_sitActionState = SitActionState::Inactive;
    g_darkLinkDrawModelData = nullptr;
    g_darkLinkAppearanceModelData.fill(nullptr);
    g_darkLinkAppearanceModelDataCount = 0;
    g_secondaryItemInputHandled = false;

    ConfigVarDesc config = CONFIG_VAR_DESC_INIT;
    config.name = "mapOpenDirection";
    config.type = CONFIG_VAR_INT;
    config.default_int = kWheelOpensDown;
    ModResult result = svc_config->register_var(mod_ctx, &config, &g_wheelOpenDirection);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register secondary wheel direction setting");
        return result;
    }

    config = CONFIG_VAR_DESC_INIT;
    config.name = "infiniteLavaWalk";
    config.type = CONFIG_VAR_BOOL;
    config.default_bool = false;
    result = svc_config->register_var(mod_ctx, &config, &g_infiniteLavaWalk);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register infinite lava walk setting");
        return result;
    }

    config = CONFIG_VAR_DESC_INIT;
    config.name = "twilightWard";
    config.type = CONFIG_VAR_BOOL;
    config.default_bool = false;
    result = svc_config->register_var(mod_ctx, &config, &g_twilightWard);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register Twilight Ward setting");
        return result;
    }

    config = CONFIG_VAR_DESC_INIT;
    config.name = "hawkOneShotDamage";
    config.type = CONFIG_VAR_BOOL;
    config.default_bool = false;
    result = svc_config->register_var(mod_ctx, &config, &g_hawkOneShotDamage);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register Hawk Grass damage setting");
        return result;
    }

    config = CONFIG_VAR_DESC_INIT;
    config.name = "hoverBootsWaterWalking";
    config.type = CONFIG_VAR_BOOL;
    config.default_bool = false;
    result = svc_config->register_var(mod_ctx, &config, &g_hoverBootsWaterWalking);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register Hover Boots water walking setting");
        return result;
    }

    config = CONFIG_VAR_DESC_INIT;
    config.name = "rupeeMagnetEnabled";
    config.type = CONFIG_VAR_BOOL;
    config.default_bool = false;
    result = svc_config->register_var(mod_ctx, &config, &g_rupeeMagnetEnabled);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register Rupee Magnet setting");
        return result;
    }

    config = CONFIG_VAR_DESC_INIT;
    config.name = "rupeeMagnetRadiusPercent";
    config.type = CONFIG_VAR_INT;
    config.default_int = kRupeeMagnetDefaultRadiusPercent;
    result = svc_config->register_var(mod_ctx, &config, &g_rupeeMagnetRadiusPercent);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register Rupee Magnet radius setting");
        return result;
    }

    config = CONFIG_VAR_DESC_INIT;
    config.name = "sitMusicTrack";
    config.type = CONFIG_VAR_INT;
    config.default_int = 0;
    result = svc_config->register_var(mod_ctx, &config, &g_sitMusicTrack);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register Sit music setting");
        return result;
    }
    int64_t sitMusicTrack = 0;
    result = svc_config->get_int(mod_ctx, g_sitMusicTrack, &sitMusicTrack);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to read Sit music setting");
        return result;
    }
    if (sitMusicTrack < 0 ||
        sitMusicTrack >= static_cast<int64_t>(kSitMusicOptionCount))
    {
        svc_log->warn(mod_ctx, "reset invalid Sit music selection to area music");
        result = svc_config->set_int(mod_ctx, g_sitMusicTrack, 0);
        if (result != MOD_OK) {
            svc_log->error(mod_ctx, "failed to reset invalid Sit music selection");
            return result;
        }
    }

    for (int button = 0; button < 2; ++button) {
        config = CONFIG_VAR_DESC_INIT;
        config.name = button == 0 ? "secondaryXAssignment" : "secondaryYAssignment";
        config.type = CONFIG_VAR_INT;
        config.default_int = dItemNo_NONE_e;
        result = svc_config->register_var(
            mod_ctx, &config, &g_secondaryAssignmentConfig[button]);
        if (result != MOD_OK) {
            svc_log->error(mod_ctx, "failed to register private secondary item assignment");
            return result;
        }
    }

    result = load_secondary_assignments();
    if (result != MOD_OK) {
        return result;
    }

    UiModsPanelDesc panel = UI_MODS_PANEL_DESC_INIT;
    panel.build = build_panel;
    result = svc_ui->register_mods_panel(mod_ctx, &panel);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register the secondary wheel settings panel");
        return result;
    }

    UiMenuTabDesc menuTab = UI_MENU_TAB_DESC_INIT;
    menuTab.label = "Robin's W&T";
    menuTab.on_selected = open_robin_settings_window;
    result = svc_ui->register_menu_tab(mod_ctx, &menuTab, &g_robinMenuTab);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register Robin's W&T menu entry");
        return result;
    }

    result = install_hooks();
    if (result != MOD_OK) {
        return result;
    }
    svc_log->info(mod_ctx, "secondary inventory wheel initialized");
    (void)error;
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    if (mDoCPd_c::getTrigX(PAD_1) == 0 && mDoCPd_c::getTrigY(PAD_1) == 0) {
        g_secondaryItemInputHandled = false;
    }
    if (g_hoverBootsTransitionPending) {
        auto* alink = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
        if (alink == nullptr || alink->mProcID != daAlink_c::PROC_BOOTS_EQUIP) {
            g_hoverBootsTransitionPending = false;
        }
    }
    update_dark_link_form_blend();
    update_sit_action();
    update_sit_music();
    update_goron_lava_contact_timer();
    update_hover_boots();
    update_hawk_grass_resource();
    update_spawned_hawk_cleanup();
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    restore_dark_link_colors();
    g_darkLinkFormActive = false;
    if (g_sitActionState == SitActionState::Seated) {
        auto* alink = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
        if (alink != nullptr && !alink->checkEventRun() &&
            alink->mProcID == daAlink_c::PROC_WAIT)
        {
            restore_sit_shape_angles(alink);
            alink->mSinkShapeOffset = 0.0f;
            alink->setSingleAnimeBase(daAlink_c::ANM_WAIT);
        }
    }
    restore_sit_area_music();
    g_sitActionState = SitActionState::Inactive;
    if (g_robinSettingsWindow != 0) {
        svc_ui->window_close(mod_ctx, g_robinSettingsWindow);
        g_robinSettingsWindow = 0;
    }
    if (g_robinMenuTab != 0) {
        svc_ui->unregister_menu_tab(mod_ctx, g_robinMenuTab);
        g_robinMenuTab = 0;
    }
    if (g_hawkGrassPhaseRequested) {
        dComIfG_resDelete(&g_hawkGrassPhase, "J_Tobi");
    }
    g_hawkGrassPhase = {};
    g_hawkGrassPhaseRequested = false;
    g_hawkGrassArchiveLoaded = false;
    g_pendingHawkGrassUse = nullptr;
    g_pendingHawkGrassButton = 0;
    g_secondaryRing = nullptr;
    g_openingSecondaryRing = false;
    g_secondaryHudOwner = nullptr;
    g_secondaryHudAssignmentsInitialized = false;
    g_secondaryHudRefreshPending.fill(false);
    g_goronTunicEquipped = false;
    g_goronLavaContactFrames = 0;
    g_hoverBootsEquipped = false;
    g_darkLinkFormActive = false;
    g_darkLinkFormBlend = 0.0f;
    g_darkLinkDrawModelData = nullptr;
    g_secondaryItemInputHandled = false;
    g_hoverGroundHeightValid = false;
    g_hoverLandingSequenceActive = false;
    g_hoverAtSurface = false;
    clear_hover_water_ground_plane();
    g_hoverLandingFrames = 0;
    g_hoverRippleEmitter = 0;
    stop_hover_magic_effects();
    g_primaryRingOpen = false;
    g_pendingPrimaryItemSync = false;
    g_deletingSecondaryRing = false;
    g_hoverBootsTransitionPending = false;
    clear_tunic_wheel_textures();
    g_keyWaitDepth = 0;
    g_mapInputDepth = 0;
    svc_log->info(mod_ctx, "secondary inventory wheel unloaded");
    return MOD_OK;
}
}
