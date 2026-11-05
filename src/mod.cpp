#include "mods/service.hpp"
#include "mods/svc/config.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/interp.h"
#include "mods/svc/log.h"
#include "mods/svc/resource.h"
#include "mods/svc/save.h"
#include "mods/svc/ui.h"

#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "JSystem/J3DGraphBase/J3DPacket.h"
#include "JSystem/J3DGraphBase/J3DShapeDraw.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphBase/J3DTexture.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "JSystem/JAudio2/JAUSectionHeap.h"
#include "JSystem/JAudio2/JASWaveArcLoader.h"
#include "JSystem/JAudio2/JASWaveInfo.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JUtility/JUTNameTab.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_arrow.h"
#include "d/actor/d_a_ni.h"
#include "d/actor/d_a_obj_iceleaf.h"
#include "d/d_a_item_static.h"
#include "d/actor/d_a_npc_tk.h"
#include "SSystem/SComponent/c_cc_d.h"
#include "SSystem/SComponent/c_cc_s.h"
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
#include "d/d_kankyo.h"
#include "d/d_pane_class.h"
#include "d/d_select_cursor.h"
#include "Z2AudioLib/Z2AudioMgr.h"
#include "Z2AudioLib/Z2SceneMgr.h"
#include "Z2AudioLib/Z2Param.h"
#include "Z2AudioLib/Z2SeqMgr.h"
#include "Z2AudioLib/Z2SoundMgr.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_leaf.h"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_ext.h"

#include <dolphin/os.h>
#include <dolphin/pad.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

DEFINE_MOD();
IMPORT_SERVICE(ConfigService, svc_config);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(InterpService, svc_interp);
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(ResourceService, svc_resource);
IMPORT_SERVICE(SaveService, svc_save);
IMPORT_SERVICE(UiService, svc_ui);

namespace {

constexpr int kWheelOpensLeft = 0;
constexpr int kWheelOpensDown = 1;
constexpr int64_t kManualJumpDisabled = 0;
constexpr int64_t kManualJumpDefaultBinding = 1;
constexpr int64_t kManualJumpDefaultHeightPercent = 100;
constexpr int64_t kManualJumpMinHeightPercent = 100;
constexpr int64_t kManualJumpMaxHeightPercent = 300;
constexpr int64_t kHoverPlaneLowerDefaultBinding = 5;
constexpr u8 kShieldSurfSlot = 0xFC;
constexpr u8 kSitSlot = 0xFD;
constexpr u8 kGoronTunicSlot = 0xFE;
constexpr u8 kShieldSurfItemId = 0xF7;
constexpr u8 kSitItemId = 0xF9;
constexpr u8 kGoronTunicItemId = 0xFB;
constexpr u8 kHawkGrassItem = 0xFD;
constexpr u8 kHoverBootsItem = 0xFC;
constexpr u8 kDarkLinkItem = 0xFA;
constexpr u8 kIceArrowItem = 0xF8;
constexpr u8 kSecondarySlotBase = 0x80;
constexpr int kHawkGrassIconTexture = 0x59;
constexpr u8 kSecondaryWheelSlotCount = MAX_ITEM_SLOTS + 3;
constexpr f32 kSecondaryIconScale = 0.55f;
constexpr f32 kSecondaryCursorScale = 0.5f;
constexpr u32 kHoverLandingRecoveryFrames = 30;
constexpr u32 kGoronLavaReturnDelayFrames = 5 * 30;
constexpr u16 kIceArrowType = 3;
constexpr u16 kIceArrowFreezeFrames = 90;
constexpr u16 kIceKeeseModelResource = 13;
constexpr f32 kIceKeeseArrowTipOffset = 95.0f;
constexpr f32 kIceKeeseArrowScale = 0.3f;
constexpr size_t kItemTextureBufferSize = 0xC00;
constexpr char kHoverBootsIconResource[] = "hover_boots_wings.bti";
constexpr char kDarkLinkIconResource[] = "dark_link.bti";
constexpr char kIceArrowIconResource[] = "ice_arrow.bti";
constexpr char kSitIconResource[] = "sit.bti";
constexpr char kSecondaryAssignmentsBlobName[] = "secondaryAssignments";
constexpr u8 kGoronTunicDisplayItem = dItemNo_WEAR_KOKIRI_e;
constexpr u32 kHawkAttackType =
    AT_TYPE_THROW_OBJ | AT_TYPE_NORMAL_SWORD | AT_TYPE_BOMB;
constexpr unsigned int kHawkIdleDespawnDelayFrames = 5 * 30;
constexpr unsigned int kHoverWaterMissToleranceFrames = 8;
constexpr u32 kDarkLinkTransitionFrames = 30;
constexpr f32 kSitModelGroundOffset = -28.0f;
constexpr f32 kSitModelForwardOffset = 65.0f;
constexpr f32 kShieldSurfBoardRaise = 15.0f;
constexpr int64_t kRupeeMagnetDefaultRadiusPercent = 100;
constexpr int64_t kRupeeMagnetMinRadiusPercent = 50;
constexpr int64_t kRupeeMagnetMaxRadiusPercent = 300;
constexpr f32 kRupeeMagnetBaseRadius = 300.0f;
constexpr u32 kSitMusicBgmId = Z2BGM_MENU_SELECT;
constexpr size_t kSitMusicOptionCount = 1;

struct SitMusicWaveMapping {
    u32 track;
    u8 wave1;
    u8 alternateWave1;
    u8 wave2 = 0;
};

struct SitMusicWaveSnapshot {
    bool active = false;
    u8 requestWave1 = 0;
    u8 requestWave2 = 0;
    u8 loadedWave1 = 0;
    u8 loadedWave2 = 0;
    u32 mainTrack = static_cast<u32>(-1);
    u8 mainStatus = 0xff;
};

constexpr SitMusicWaveMapping kSitMusicWaveMappings[] = {
    {Z2BGM_MENU_SELECT, 0, 0},
};

enum class SecondaryIconContext : u8 {
    None,
    Sit,
    GoronTunic,
    HawkGrass,
    HoverBoots,
    DarkLink,
    IceArrow,
};

enum class SitActionState : u8 {
    Inactive,
    Seated,
};

ConfigVarHandle g_wheelOpenDirection = 0;
ConfigVarHandle g_manualJumpBinding = 0;
ConfigVarHandle g_manualJumpHeightPercent = 0;
ConfigVarHandle g_hoverPlaneLowerBinding = 0;
ConfigVarHandle g_infiniteLavaWalk = 0;
ConfigVarHandle g_twilightWard = 0;
ConfigVarHandle g_hawkOneShotDamage = 0;
ConfigVarHandle g_hoverBootsWaterWalking = 0;
ConfigVarHandle g_shieldSurfRBoost = 0;
ConfigVarHandle g_rupeeMagnetEnabled = 0;
ConfigVarHandle g_rupeeMagnetRadiusPercent = 0;
ConfigVarHandle g_sitMusicTrack = 0;
std::array<std::array<char, 48>, kSitMusicOptionCount> g_sitMusicOptionLabelStorage{};
std::array<const char*, kSitMusicOptionCount> g_sitMusicOptionLabels{};
bool g_sitMusicOptionsInitialized = false;
u32 g_sitMusicActiveTrack = 0;
bool g_sitMusicAreaFadePending = false;
u16 g_sitMusicAreaFadeFrames = 0;
bool g_sitMusicAreaFadeTimeoutReported = false;
u32 g_sitMusicLastStartedTrack = 0;
u32 g_sitMusicReportedOverrideTrack = 0;
int64_t g_sitMusicLastLoggedSelection = -1;
bool g_sitMusicConfigReadFailureReported = false;
bool g_sitMusicAudioManagerUnavailableReported = false;
SitMusicWaveSnapshot g_sitMusicWaveSnapshot{};
u32 g_sitMusicWaveTrack = 0;
bool g_sitMusicBattleOverrideActive = false;
bool g_sitMusicOriginalBattleBgmOff = true;
bool g_sitMusicRestoreFailureReported = false;
u8 g_sitMusicResolvedWave1 = 0;
u32 g_sitMusicReportedFailureTrack = 0;
u32 g_sitMusicWaveFailureTrack = 0;
std::array<bool, 256> g_sitMusicOwnedWaves{};
std::array<bool, 256> g_sitMusicWaveEraseFailureReported{};
std::array<bool, 256> g_sitMusicWaveReleaseScheduled{};
std::array<u8, 256> g_sitMusicWaveReleaseFrames{};
UiMenuTabHandle g_robinMenuTab = 0;
UiWindowHandle g_robinSettingsWindow = 0;
SaveObserverHandle g_secondarySaveObserver = 0;
dMenu_Ring_c* g_secondaryRing = nullptr;
bool g_openingSecondaryRing = false;
bool g_goronTunicEquipped = false;
u32 g_goronLavaContactFrames = 0;
bool g_hoverBootsEquipped = false;
bool g_shieldSurfActive = false;
bool g_shieldSurfStartPending = false;
bool g_shieldSurfDismountPending = false;
fpc_ProcID g_shieldSurfBoardId = fpcM_ERROR_PROCESS_ID_e;
unsigned int g_shieldSurfStartWaitFrames = 0;
bool g_manualCuccoGlideActive = false;
u32 g_manualGlideAuraEmitters[2][3] = {};
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
struct IceArrowFrozenTarget {
    fpc_ProcID actorId = fpcM_ERROR_PROCESS_ID_e;
    J3DModel* model = nullptr;
    cXyz frozenPosition{};
    csXyz frozenAngle{};
    csXyz frozenShapeAngle{};
    f32 originalGravity = 0.0f;
    f32 originalMaxFallSpeed = 0.0f;
    GXColorS10 originalTevColor{};
    u16 framesRemaining = 0;
    bool shaderColorCaptured = false;
    bool active = false;
};
std::array<IceArrowFrozenTarget, 32> g_iceArrowFrozenTargets{};
cCcD_Obj* g_iceArrowSuppressedAttackCollider = nullptr;
fpc_ProcID g_iceArrowExecutingActorId = fpcM_ERROR_PROCESS_ID_e;
fpc_ProcID g_iceArrowPreviousExecutingActorIds[16]{};
u8 g_iceArrowActorExecuteDepth = 0;
bool g_iceArrowSelected = false;
bool g_secondaryItemInputHandled = false;
bool g_manualJumpConfigReadFailureReported = false;
bool g_manualJumpHeightConfigReadFailureReported = false;
bool g_hoverPlaneLowerBindingConfigReadFailureReported = false;
bool g_hoverPlaneLowerBindingWasHeld = false;
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
request_of_phase_process_class g_iceKeesePhase{};
JKRExpHeap* g_iceKeeseHeap = nullptr;
J3DModel* g_iceKeeseModel = nullptr;
daAlink_c* g_pendingHawkGrassUse = nullptr;
u8 g_pendingHawkGrassButton = 0;
bool g_hawkGrassPhaseRequested = false;
bool g_hawkGrassArchiveLoaded = false;
bool g_iceKeesePhaseRequested = false;
bool g_iceKeeseArchiveLoaded = false;
bool g_iceKeeseLoadFailed = false;
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
    slots[3] = kIceArrowItem;
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
bool g_secondaryRingInterpolationErrorLogged = false;
std::array<J2DPicture*, 3> g_tunicWheelItemTex{};
std::array<ResTIMG*, 3> g_tunicWheelItemBuf{};
JKRExpHeap* g_tunicWheelItemHeap = nullptr;
J2DPicture* g_sitWheelItemTex = nullptr;
ResTIMG* g_sitWheelItemBuf = nullptr;
JKRExpHeap* g_sitWheelItemHeap = nullptr;
std::array<J2DPicture*, 3> g_shieldSurfWheelItemTex{};
std::array<ResTIMG*, 3> g_shieldSurfWheelItemBuf{};
JKRExpHeap* g_shieldSurfWheelItemHeap = nullptr;

// Feature modules share this translation unit's private state and hook declarations.
#include "features/core_helpers.cpp"
#include "features/hover_boots.cpp"
#include "features/secondary_wheel.cpp"
#include "features/world_hooks.cpp"
#include "features/goron_tunic.cpp"
#include "features/dark_link.cpp"
#include "features/character_actions.cpp"
#include "features/sit_music.cpp"
#include "features/shield_surf.cpp"
#include "features/manual_jump.cpp"
#include "features/settings.cpp"
#include "features/ice_arrow.cpp"

ModResult install_hooks() {
    ADD_PRE(OpenExplain, suppress_secondary_explain);
    ADD_POST(SetNameString, name_secondary_item);
    ADD_PRE(RingSetSelectItem, prepare_secondary_equip_icon);
    ADD_POST(RingSetSelectItem, finish_secondary_equip_icon);
    ADD_POST(RingSetJumpItem, clear_secondary_assignment_for_vanilla_ring);
    ADD_PRE(RingIsMixItemOn, show_ice_arrow_combo_on_prompt);
    ADD_PRE(RingIsMixItemOff, show_ice_arrow_combo_off_prompt);
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
    ADD_PRE(CheckGrabRooster, emulate_manual_cucco_glide_check);
    ADD_POST(SetStickData, suppress_sit_stick_input);
    ADD_PRE(CheckSnowCode, allow_shield_surf_on_any_ground);
    ADD_PRE(CheckShieldDraw, suppress_player_shield_draw_during_surf);
    ADD_PRE(CheckNextActionBow, suppress_bow_action_during_shield_surf);
    ADD_PRE(RideGetOff, release_shield_surf_ride_safely);
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
    ADD_PRE(PlayerExecute, update_ice_arrow_freezes);
    ADD_POST(PlayerExecute, process_shield_surf_transition);
    ADD_PRE(ProcAutoJump, release_manual_cucco_glide_on_a);
    ADD_POST(BoardCommon, boost_shield_surf_speed);
    ADD_PRE(IceLeafExecute, keep_shield_surf_board_in_ride_mode);
    ADD_PRE(IceLeafDraw, draw_shield_surf_on_native_board);
    ADD_POST(MakeArrow, set_ice_arrow_actor_type_after_creation);
    ADD_POST(ArrowDraw, draw_ice_keese_on_held_arrow);
    ADD_PRE(ChangeArrowType, apply_ice_arrow_bow_selection);
    ADD_PRE(ArrowAtHitCallBack, register_ice_arrow_hit);
    ADD_PRE(MorfFrameUpdate, freeze_ice_arrow_target_animation);
    ADD_PRE(LeafExecute, begin_ice_arrow_actor_execute);
    ADD_POST(LeafExecute, hold_ice_arrow_target_in_place);
    ADD_POST(LeafExecute, end_ice_arrow_actor_execute);
    ADD_PRE(CollisionSet, suppress_ice_frozen_enemy_attack);
    ADD_POST(CollisionSet, restore_ice_frozen_enemy_attack);
    ADD_PRE(ProcCoLavaReturnInit, delay_goron_lava_return);
    ADD_PRE(CheckRestartRoom, suppress_water_surface_restart);
    ADD_PRE(CheckSceneChange, suppress_submerged_water_exit);
    ADD_POST(ModelCalc, reapply_tunic_override_after_model_calc);
    ADD_PRE(ModelCalc, offset_sit_model_forward_before_calc);
    ADD_PRE(ModelDraw, mark_dark_link_model_for_draw);
    ADD_POST(LightTevColorTypeMaji, reapply_tunic_override_after_lighting);
    ADD_POST(LightTevColorTypeMaji, apply_dark_link_after_lighting);
    ADD_POST(SettingTevStruct, apply_ice_arrow_shader_after_tev_setup);
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
    g_shieldSurfActive = false;
    g_shieldSurfStartPending = false;
    g_shieldSurfDismountPending = false;
    g_shieldSurfBoardId = fpcM_ERROR_PROCESS_ID_e;
    g_shieldSurfStartWaitFrames = 0;
    g_manualCuccoGlideActive = false;
    stop_manual_glide_aura();
    g_iceArrowSelected = false;
    g_iceArrowFrozenTargets.fill({});
    g_sitActionState = SitActionState::Inactive;
    g_sitMusicActiveTrack = 0;
    g_sitMusicAreaFadePending = false;
    g_sitMusicAreaFadeFrames = 0;
    g_sitMusicAreaFadeTimeoutReported = false;
    g_sitMusicLastStartedTrack = 0;
    g_sitMusicReportedOverrideTrack = 0;
    g_sitMusicLastLoggedSelection = -1;
    g_sitMusicConfigReadFailureReported = false;
    g_sitMusicAudioManagerUnavailableReported = false;
    g_sitMusicReportedFailureTrack = 0;
    g_sitMusicWaveFailureTrack = 0;
    g_darkLinkDrawModelData = nullptr;
    g_darkLinkAppearanceModelData.fill(nullptr);
    g_darkLinkAppearanceModelDataCount = 0;
    g_secondaryItemInputHandled = false;
    g_manualJumpConfigReadFailureReported = false;
    g_manualJumpHeightConfigReadFailureReported = false;
    g_hoverPlaneLowerBindingConfigReadFailureReported = false;
    g_hoverPlaneLowerBindingWasHeld = false;
    g_secondaryRingInterpolationErrorLogged = false;

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
    config.name = "manualJumpBinding";
    config.type = CONFIG_VAR_INT;
    config.default_int = kManualJumpDefaultBinding;
    result = svc_config->register_var(mod_ctx, &config, &g_manualJumpBinding);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register manual jump binding setting");
        return result;
    }

    config = CONFIG_VAR_DESC_INIT;
    config.name = "manualJumpHeightPercent";
    config.type = CONFIG_VAR_INT;
    config.default_int = kManualJumpDefaultHeightPercent;
    result = svc_config->register_var(mod_ctx, &config, &g_manualJumpHeightPercent);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register manual jump height setting");
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
    config.name = "hoverPlaneLowerBinding";
    config.type = CONFIG_VAR_INT;
    config.default_int = kHoverPlaneLowerDefaultBinding;
    result = svc_config->register_var(mod_ctx, &config, &g_hoverPlaneLowerBinding);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register Hover Boots lower-plane binding setting");
        return result;
    }

    config = CONFIG_VAR_DESC_INIT;
    config.name = "shieldSurfRBoost";
    config.type = CONFIG_VAR_BOOL;
    config.default_bool = false;
    result = svc_config->register_var(mod_ctx, &config, &g_shieldSurfRBoost);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register Shield Surf R-button speed boost setting");
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
    if (sitMusicTrack != 0) {
        svc_log->warn(mod_ctx, "reset legacy Sit music selection to File Select");
        result = svc_config->set_int(mod_ctx, g_sitMusicTrack, 0);
        if (result != MOD_OK) {
            svc_log->error(mod_ctx, "failed to reset invalid Sit music selection");
            return result;
        }
    }

    result = svc_save->observe_saves(
        mod_ctx, clear_secondary_items_for_new_save, restore_secondary_items_for_save,
        persist_secondary_items_for_save, nullptr, &g_secondarySaveObserver);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to observe game save lifecycle");
        return result;
    }
    restore_secondary_items_for_save(mod_ctx, 0, nullptr);

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
    update_manual_cucco_glide(
        static_cast<daAlink_c*>(dComIfGp_getLinkPlayer()));
    update_hawk_grass_resource();
    update_ice_keese_resource();
    update_spawned_hawk_cleanup();
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    g_manualCuccoGlideActive = false;
    stop_manual_glide_aura();
    if (g_secondarySaveObserver != 0) {
        const ModResult result =
            svc_save->unobserve_saves(mod_ctx, g_secondarySaveObserver);
        if (result != MOD_OK) {
            svc_log->error(mod_ctx, "failed to unregister game save observer");
        }
        g_secondarySaveObserver = 0;
    }
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
    stop_sit_music_track();
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
    if (g_iceKeeseHeap != nullptr) {
        mDoExt_destroyExpHeap(g_iceKeeseHeap);
        g_iceKeeseHeap = nullptr;
        g_iceKeeseModel = nullptr;
    }
    if (g_iceKeesePhaseRequested) {
        dComIfG_resDelete(&g_iceKeesePhase, "E_ib");
    }
    g_hawkGrassPhase = {};
    g_iceKeesePhase = {};
    g_hawkGrassPhaseRequested = false;
    g_hawkGrassArchiveLoaded = false;
    g_iceKeesePhaseRequested = false;
    g_iceKeeseArchiveLoaded = false;
    g_iceKeeseLoadFailed = false;
    g_pendingHawkGrassUse = nullptr;
    g_pendingHawkGrassButton = 0;
    forget_secondary_ring_pose(g_secondaryRing);
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
    g_iceArrowSelected = false;
    g_iceArrowFrozenTargets.fill({});
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
