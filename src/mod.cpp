#include "mods/service.hpp"
#include "mods/svc/config.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.h"
#include "mods/svc/resource.h"
#include "mods/svc/ui.h"

#include "JSystem/J3DGraphBase/J3DMaterial.h"
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
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_controller_pad.h"

#include <dolphin/os.h>

#include <algorithm>
#include <array>
#include <cmath>
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
constexpr u8 kGoronTunicSlot = 0xFE;
constexpr u8 kGoronTunicItemId = 0xFB;
constexpr u8 kHawkGrassItem = 0xFD;
constexpr u8 kHoverBootsItem = 0xFC;
constexpr u8 kSecondarySlotBase = 0x80;
constexpr int kHawkGrassIconTexture = 0x59;
constexpr u8 kSecondaryWheelSlotCount = MAX_ITEM_SLOTS + 1;
constexpr f32 kSecondaryIconScale = 0.55f;
constexpr f32 kSecondaryCursorScale = 0.5f;
constexpr u32 kHoverLandingRecoveryFrames = 30;
constexpr size_t kItemTextureBufferSize = 0xC00;
constexpr char kHoverBootsIconResource[] = "hover_boots_wings.bti";
constexpr u8 kGoronTunicDisplayItem = dItemNo_WEAR_KOKIRI_e;
constexpr u32 kHawkAttackType =
    AT_TYPE_THROW_OBJ | AT_TYPE_NORMAL_SWORD | AT_TYPE_BOMB;
constexpr unsigned int kLavaReturnDelayFrames = 5 * 30;
constexpr unsigned int kHawkIdleDespawnDelayFrames = 5 * 30;
constexpr unsigned int kHoverWaterMissToleranceFrames = 8;
constexpr int64_t kRupeeMagnetDefaultRadiusPercent = 100;
constexpr int64_t kRupeeMagnetMinRadiusPercent = 50;
constexpr int64_t kRupeeMagnetMaxRadiusPercent = 300;
constexpr f32 kRupeeMagnetBaseRadius = 300.0f;

enum class SecondaryIconContext : u8 {
    None,
    GoronTunic,
    HawkGrass,
    HoverBoots,
};

ConfigVarHandle g_wheelOpenDirection = 0;
ConfigVarHandle g_infiniteLavaWalk = 0;
ConfigVarHandle g_twilightWard = 0;
ConfigVarHandle g_hawkOneShotDamage = 0;
ConfigVarHandle g_hoverBootsWaterWalking = 0;
ConfigVarHandle g_rupeeMagnetEnabled = 0;
ConfigVarHandle g_rupeeMagnetRadiusPercent = 0;
std::array<ConfigVarHandle, 2> g_secondaryAssignmentConfig{};
UiMenuTabHandle g_robinMenuTab = 0;
UiWindowHandle g_robinSettingsWindow = 0;
dMenu_Ring_c* g_secondaryRing = nullptr;
bool g_openingSecondaryRing = false;
bool g_goronTunicEquipped = false;
bool g_hoverBootsEquipped = false;
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
unsigned int g_lavaSinkFrames = 0;
bool g_primaryRingOpen = false;
bool g_pendingPrimaryItemSync = false;
bool g_deletingSecondaryRing = false;
bool g_hoverBootsTransitionPending = false;
int g_keyWaitDepth = 0;
int g_mapInputDepth = 0;
u8 g_savedGroundAtt0 = 0;
u8 g_savedGroundAtt1 = 0;
daAlink_c* g_sandShapeActor = nullptr;
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
    if (itemId == kGoronTunicItemId) {
        return kGoronTunicDisplayItem;
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
    if (slotId == kGoronTunicSlot) {
        return SecondaryIconContext::GoronTunic;
    }
    if (isHawkGrassSlot(slotId)) {
        return SecondaryIconContext::HawkGrass;
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
    svc_log->info(mod_ctx, "Hawk Grass equipped");
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
    return slotId == dItemNo_NONE_e || slotId == kGoronTunicSlot ||
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
    return g_goronTunicEquipped && alink != nullptr && alink->field_0x2fbc == 6 &&
           alink->mWaterY != -G_CM3D_F_INF && alink->mLinkAcch.ChkGroundHit() &&
           alink->current.pos.y <= alink->mWaterY + 1.0f &&
           alink->current.pos.y >= alink->mWaterY - 1.0f;
}

void update_lava_return_timer() {
    auto* alink = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    if (infiniteLavaWalkEnabled() && is_lava_surface_contact(alink)) {
        alink->mSinkShapeOffset = 0.0f;
        g_lavaSinkFrames = 0;
        return;
    }
    if (!is_lava_surface_contact(alink) || alink->mSinkShapeOffset >= -0.5f ||
        alink->mProcID == daAlink_c::PROC_LAVA_RETURN)
    {
        g_lavaSinkFrames = 0;
        return;
    }

    if (g_lavaSinkFrames < kLavaReturnDelayFrames) {
        ++g_lavaSinkFrames;
    }
    if (g_lavaSinkFrames == kLavaReturnDelayFrames &&
        alink->procCoLavaReturnInit(FALSE) != 0)
    {
        g_lavaSinkFrames = 0;
        svc_log->info(mod_ctx, "Goron Tunic: triggered vanilla lava return");
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
    g_secondaryWheelSlotIds[MAX_ITEM_SLOTS] = kGoronTunicSlot;

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
    if (ring->mCurrentSlot == MAX_ITEM_SLOTS) {
        ring->mCurrentSlot = MAX_ITEM_SLOTS - 1;
        ring->mItemSlotPosX[MAX_ITEM_SLOTS - 1] =
            g_secondaryWheelSlotPosX[MAX_ITEM_SLOTS];
        ring->mItemSlotPosY[MAX_ITEM_SLOTS - 1] =
            g_secondaryWheelSlotPosY[MAX_ITEM_SLOTS];
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

void replace_hover_boots_icon(ModContext*, void* args, void*, void*) {
    if (g_secondaryIconContext != SecondaryIconContext::HoverBoots ||
        mods::arg<u8>(args, 0) != dItemNo_WOOD_STICK_e)
    {
        return;
    }

    ResourceBuffer icon = RESOURCE_BUFFER_INIT;
    const ModResult result = svc_resource->load(mod_ctx, kHoverBootsIconResource, &icon);
    if (result != MOD_OK || icon.data == nullptr || icon.size == 0) {
        svc_log->error(mod_ctx, "failed to load the Hover Boots wing icon");
        svc_resource->free(mod_ctx, &icon);
        return;
    }
    if (icon.size > kItemTextureBufferSize) {
        svc_log->error(mod_ctx, "Hover Boots wing icon exceeds the item texture buffer");
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
    if (g_secondaryIconContext != SecondaryIconContext::HoverBoots) {
        mods::arg_ref<u8>(args, 2) = secondaryDisplayItemForSlot(slot);
        return HOOK_CONTINUE;
    }
    g_secondaryIconContext = SecondaryIconContext::None;

    auto* meter = mods::arg<dMeter2Draw_c*>(args, 0);
    if (meter == nullptr || meter->mpItemXY[button] == nullptr ||
        meter->mpItemXYPane[button] == nullptr)
    {
        svc_log->error(mod_ctx, "failed to access the X/Y HUD panes for Hover Boots");
        return HOOK_SKIP_ORIGINAL;
    }

    ResourceBuffer icon = RESOURCE_BUFFER_INIT;
    const ModResult result = svc_resource->load(mod_ctx, kHoverBootsIconResource, &icon);
    if (result != MOD_OK || icon.data == nullptr || icon.size == 0) {
        svc_log->error(mod_ctx, "failed to load the Hover Boots X/Y HUD icon");
        svc_resource->free(mod_ctx, &icon);
        return HOOK_SKIP_ORIGINAL;
    }
    if (icon.size < sizeof(ResTIMG) || icon.size > kItemTextureBufferSize) {
        svc_log->error(mod_ctx, "Hover Boots X/Y HUD icon has an invalid texture size");
        svc_resource->free(mod_ctx, &icon);
        return HOOK_SKIP_ORIGINAL;
    }

    const u8 textureIndex = meter->field_0x76c[button] == 0 ? 1 : 0;
    meter->field_0x76c[button] = textureIndex;
    ResTIMG* texture = meter->mpItemXYTex[button][textureIndex][0];
    if (texture == nullptr) {
        svc_log->error(mod_ctx, "failed to access the Hover Boots X/Y HUD texture buffer");
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
        svc_log->error(mod_ctx, "failed to apply the Hover Boots X/Y HUD icon");
        return HOOK_SKIP_ORIGINAL;
    }
    picture->setBlackWhite(
        JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 255, 255, 255));
    meter->mpItemXYPane[button]->hide();

    const f32 scale = g_drawHIO.mItemScaleAdjustON
                          ? g_drawHIO.mItemScalePercent / 100.0f
                          : 1.0f;
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

    if (alink == nullptr || !g_goronTunicEquipped) {
        return HOOK_CONTINUE;
    }

    const bool groundedLava =
        !alink->checkMagneBootsOn() && alink->mLinkAcch.ChkGroundHit() &&
        alink->mGndPolyAtt1 == 2;
    if (groundedLava) {
        *static_cast<BOOL*>(retval) = FALSE;
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

void treat_lava_as_surface(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!g_goronTunicEquipped || alink == nullptr || alink->field_0x2fbc != 6 ||
        alink->mWaterY == -G_CM3D_F_INF || alink->speed.y > 0.0f ||
        alink->current.pos.y > alink->mWaterY ||
        alink->current.pos.y < alink->mWaterY - alink->mHeight)
    {
        return;
    }

    alink->current.pos.y = alink->mWaterY;
    alink->speed.y = 0.0f;
    alink->mLinkAcch.m_ground_h = alink->mWaterY;
    alink->mLinkAcch.SetGroundHit();
    alink->offModeFlg(daAlink_c::MODE_SWIMMING);
    alink->offNoResetFlg0(daAlink_c::FLG0_UNK_80);
    alink->mGndPolyAtt0 = 3;
    alink->mGndPolyAtt1 = 1;
    if (infiniteLavaWalkEnabled()) {
        alink->mSinkShapeOffset = 0.0f;
    }
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
    if (!is_lava_surface_contact(alink)) {
        return;
    }

    alink->mGndPolyAtt1 = 1;
    alink->mGroundCode = 3;
    alink->mLinkAcch.m_ground_h = alink->mWaterY;
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

    if (infiniteLavaWalkEnabled()) {
        alink->mSinkShapeOffset = 0.0f;
        g_lavaSinkFrames = 0;
        return HOOK_SKIP_ORIGINAL;
    }

    g_sandShapeActor = alink;
    g_savedGroundAtt0 = alink->mGndPolyAtt0;
    g_savedGroundAtt1 = alink->mGndPolyAtt1;
    alink->mGndPolyAtt0 = 3;
    alink->mGndPolyAtt1 = 1;
    return HOOK_CONTINUE;
}

void finish_lava_sinking(ModContext*, void*, void*, void*) {
    if (g_sandShapeActor != nullptr) {
        g_sandShapeActor->mGndPolyAtt0 = g_savedGroundAtt0;
        g_sandShapeActor->mGndPolyAtt1 = g_savedGroundAtt1;
        g_sandShapeActor = nullptr;
    }
}

HookAction suppress_lava_submersion(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (g_goronTunicEquipped && alink != nullptr && alink->field_0x2fbc == 6 &&
        alink->mWaterY != -G_CM3D_F_INF && alink->current.pos.y >= alink->mWaterY - 1.0f)
    {
        *static_cast<bool*>(retval) = false;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction suppress_lava_damage(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (g_goronTunicEquipped && alink != nullptr && alink->field_0x2fbc == 6) {
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
    auto* material = model->getMaterialNodePointer(index);
    auto* color = material != nullptr ? material->getTevColor(1) : nullptr;
    if (color == nullptr) {
        return;
    }

    color->r = 72;
    color->g = 0;
    color->b = 0;
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

void tint_goron_tunic(ModContext*, void* args, void*, void*) {
    if (!g_goronTunicEquipped) {
        return;
    }

    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || alink->field_0x064C == nullptr) {
        return;
    }

    tint_clothing_model(alink->field_0x064C);
    tint_clothing_model(alink->mpLinkFaceModel != nullptr
                            ? alink->mpLinkFaceModel->getModelData()
                            : nullptr);
    tint_clothing_model(alink->mpLinkHatModel != nullptr
                            ? alink->mpLinkHatModel->getModelData()
                            : nullptr);
}

void reapply_tunic_override_after_model_calc(ModContext*, void* args, void*, void*) {
    if (!g_goronTunicEquipped) {
        return;
    }

    auto* model = mods::arg<J3DModel*>(args, 1);
    tint_clothing_model(model != nullptr ? model->getModelData() : nullptr);
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

void update_secondary_assigned_item_input(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || g_secondaryRing != nullptr || g_primaryRingOpen ||
        alink->checkEventRun())
    {
        return;
    }

    const u8 pressedButtons = static_cast<u8>(
        (secondaryItemAssigned(0) && mDoCPd_c::getTrigX(PAD_1) != 0
             ? daAlink_c::BTN_X : 0) |
        (secondaryItemAssigned(1) && mDoCPd_c::getTrigY(PAD_1) != 0
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
    case kGoronTunicItemId:
        g_goronTunicEquipped = !g_goronTunicEquipped;
        g_lavaSinkFrames = 0;
        svc_log->info(mod_ctx, g_goronTunicEquipped ? "Goron Tunic equipped" :
                                                     "Goron Tunic unequipped");
        break;
    case kHawkGrassItem:
        begin_secondary_hawk_grass_use(alink, static_cast<u8>(button));
        break;
    case kHoverBootsItem:
        begin_hover_boots_toggle(alink);
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
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_DROPDOWN;
    control.label = "Open Secondary Wheel Bind";
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
    control.kind = UI_CONTROL_TOGGLE;
    control.label = "Goron Tunic: Indefinite Lava Walking";
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
        "When the Magic Armor is worn, hits still deal damage but do not interrupt Link, stun him, "
        "or knock him back.";
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = g_twilightWard;
    result = svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
    if (result != MOD_OK) {
        return result;
    }

    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE;
    control.label = "Hawk Grass: One-hit Attacks";
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
    control.label = "Hover Boots: Water Walk Instead";
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
    control.help_rml = "Collect nearby rupees automatically. Configurable radius.";
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = g_rupeeMagnetEnabled;
    result = svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
    if (result != MOD_OK) {
        return result;
    }

    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_NUMBER;
    control.label = "Rupee Magnet: Radius";
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
DEFINE_HOOK(&daAlink_c::checkSceneChange, CheckSceneChange);
DEFINE_HOOK(&daAlink_c::modelCalc, ModelCalc);
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
    ADD_POST(ReadItemTexture, replace_hover_boots_icon);
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
    ADD_PRE(SetSandShapeOffset, prepare_lava_sinking);
    ADD_POST(SetSandShapeOffset, finish_lava_sinking);
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
    ADD_PRE(CheckRestartRoom, suppress_water_surface_restart);
    ADD_PRE(CheckSceneChange, suppress_submerged_water_exit);
    ADD_POST(ModelCalc, reapply_tunic_override_after_model_calc);
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
    update_lava_return_timer();
    update_hover_boots();
    update_hawk_grass_resource();
    update_spawned_hawk_cleanup();
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
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
    g_hoverBootsEquipped = false;
    g_secondaryItemInputHandled = false;
    g_hoverGroundHeightValid = false;
    g_hoverLandingSequenceActive = false;
    g_hoverAtSurface = false;
    clear_hover_water_ground_plane();
    g_hoverLandingFrames = 0;
    g_hoverRippleEmitter = 0;
    stop_hover_magic_effects();
    g_lavaSinkFrames = 0;
    g_primaryRingOpen = false;
    g_pendingPrimaryItemSync = false;
    g_deletingSecondaryRing = false;
    g_hoverBootsTransitionPending = false;
    g_sandShapeActor = nullptr;
    clear_tunic_wheel_textures();
    g_keyWaitDepth = 0;
    g_mapInputDepth = 0;
    svc_log->info(mod_ctx, "secondary inventory wheel unloaded");
    return MOD_OK;
}
}
