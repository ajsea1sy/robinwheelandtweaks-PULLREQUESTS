bool wheelOpensDown() {
    int64_t value = kWheelOpensDown;
    return g_wheelOpenDirection != 0 &&
           svc_config->get_int(mod_ctx, g_wheelOpenDirection, &value) == MOD_OK &&
           value == kWheelOpensDown;
}

int64_t manualJumpBinding() {
    int64_t value = kManualJumpDefaultBinding;
    if (g_manualJumpBinding == 0 ||
        svc_config->get_int(mod_ctx, g_manualJumpBinding, &value) != MOD_OK)
    {
        if (!g_manualJumpConfigReadFailureReported) {
            svc_log->warn(mod_ctx, "could not read manual jump binding config");
            g_manualJumpConfigReadFailureReported = true;
        }
        return kManualJumpDisabled;
    }

    g_manualJumpConfigReadFailureReported = false;
    return std::clamp(value, kManualJumpDisabled, static_cast<int64_t>(5));
}

int64_t manualJumpHeightPercent() {
    int64_t value = kManualJumpDefaultHeightPercent;
    if (g_manualJumpHeightPercent == 0 ||
        svc_config->get_int(mod_ctx, g_manualJumpHeightPercent, &value) != MOD_OK)
    {
        if (!g_manualJumpHeightConfigReadFailureReported) {
            svc_log->warn(mod_ctx, "could not read manual jump height config");
            g_manualJumpHeightConfigReadFailureReported = true;
        }
        return kManualJumpDefaultHeightPercent;
    }

    g_manualJumpHeightConfigReadFailureReported = false;
    return std::clamp(value, kManualJumpMinHeightPercent, kManualJumpMaxHeightPercent);
}

int64_t glideBowSlowTimePercent() {
    int64_t value = kGlideBowSlowTimeDefaultPercent;
    if (g_glideBowSlowTimePercent == 0 ||
        svc_config->get_int(mod_ctx, g_glideBowSlowTimePercent, &value) != MOD_OK)
    {
        if (!g_glideBowSlowTimeConfigReadFailureReported) {
            svc_log->warn(mod_ctx, "could not read glide bow slow time config");
            g_glideBowSlowTimeConfigReadFailureReported = true;
        }
        return kGlideBowSlowTimeDefaultPercent;
    }

    g_glideBowSlowTimeConfigReadFailureReported = false;
    return std::clamp(
        value, kGlideBowSlowTimeMinPercent, kGlideBowSlowTimeMaxPercent);
}

int64_t hoverPlaneLowerBinding() {
    int64_t value = kHoverPlaneLowerDefaultBinding;
    if (g_hoverPlaneLowerBinding == 0 ||
        svc_config->get_int(mod_ctx, g_hoverPlaneLowerBinding, &value) != MOD_OK)
    {
        if (!g_hoverPlaneLowerBindingConfigReadFailureReported) {
            svc_log->warn(mod_ctx, "could not read Hover Boots lower-plane binding config");
            g_hoverPlaneLowerBindingConfigReadFailureReported = true;
        }
        return kHoverPlaneLowerDefaultBinding;
    }

    g_hoverPlaneLowerBindingConfigReadFailureReported = false;
    return std::clamp(value, static_cast<int64_t>(0), static_cast<int64_t>(5));
}

bool hoverPlaneLowerBindingHeld(int64_t binding) {
    const u32 buttons = mDoCPd_c::getHold(PAD_1);
    const u32 extendedButtons = JUTGamePad::mPadStatus[PAD_1].extButton;
    switch (binding) {
    case 0:
        return (buttons & PAD_TRIGGER_L) != 0;
    case 1:
        return (buttons & PAD_TRIGGER_L) == 0 && mDoCPd_c::getAnalogL(PAD_1) >= 0.5f;
    case 2:
        return (extendedButtons & PAD_BUTTON_LEFT_STICK) != 0;
    case 3:
        return (buttons & PAD_TRIGGER_R) != 0;
    case 4:
        return (buttons & PAD_TRIGGER_R) == 0 && mDoCPd_c::getAnalogR(PAD_1) >= 0.5f;
    case 5:
        return (extendedButtons & PAD_BUTTON_RIGHT_STICK) != 0;
    default:
        return false;
    }
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

bool shieldSurfRBoostEnabled() {
    return get_bool_option(g_shieldSurfRBoost, false);
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

    constexpr char kFileSelectTrackLabel[] = "File Select";
    std::memcpy(
        g_sitMusicOptionLabelStorage[0].data(),
        kFileSelectTrackLabel,
        sizeof(kFileSelectTrackLabel));
    g_sitMusicOptionLabels[0] = g_sitMusicOptionLabelStorage[0].data();
    g_sitMusicOptionsInitialized = true;
}

int64_t sitMusicSelection() {
    int64_t value = 0;
    if (g_sitMusicTrack == 0 ||
        svc_config->get_int(mod_ctx, g_sitMusicTrack, &value) != MOD_OK)
    {
        if (!g_sitMusicConfigReadFailureReported) {
            svc_log->warn(mod_ctx, "could not read Sit music selector config");
            g_sitMusicConfigReadFailureReported = true;
        }
        return -1;
    }
    g_sitMusicConfigReadFailureReported = false;
    if (value != 0) {
        return -1;
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

u8 shield_surf_display_item() {
    const u8 shield = dComIfGs_getSelectEquipShield();
    return shield == dItemNo_NONE_e ? dItemNo_SHIELD_e : shield;
}

u8 secondaryItemIdForSlot(u8 slotId) {
    if (slotId == kShieldSurfSlot) {
        return kShieldSurfItemId;
    }
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
    if (itemId == kShieldSurfItemId) {
        return shield_surf_display_item();
    }
    if (itemId == kSitItemId) {
        return dItemNo_WOOD_STICK_e;
    }
    if (itemId == kGoronTunicItemId) {
        return kGoronTunicDisplayItem;
    }
    if (itemId == kDarkLinkItem || itemId == kIceArrowItem) {
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
    if (slotId >= kSecondarySlotBase &&
        slotId < kSecondarySlotBase + MAX_ITEM_SLOTS &&
        g_secondarySlots[slotId - kSecondarySlotBase] == kIceArrowItem)
    {
        return SecondaryIconContext::IceArrow;
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

void update_ice_keese_resource() {
    if ((!g_iceArrowSelected && !g_iceKeesePhaseRequested) ||
        g_iceKeeseArchiveLoaded || g_iceKeeseLoadFailed)
    {
        return;
    }

    g_iceKeesePhaseRequested = true;
    const cPhs_Step phase = dComIfG_resLoad(&g_iceKeesePhase, "E_ib");
    if (phase == cPhs_COMPLEATE_e) {
        g_iceKeeseArchiveLoaded = true;
        auto* modelData = static_cast<J3DModelData*>(
            dComIfG_getObjectRes("E_ib", kIceKeeseModelResource));
        if (modelData == nullptr) {
            svc_log->error(mod_ctx, "Failed to get the Ice Keese model resource");
            g_iceKeeseLoadFailed = true;
            return;
        }

        g_iceKeeseHeap = JKRExpHeap::create(0x40000, mDoExt_getGameHeap(), true);
        if (g_iceKeeseHeap == nullptr) {
            svc_log->error(mod_ctx, "Failed to create the Ice Keese model heap");
            g_iceKeeseLoadFailed = true;
            return;
        }

        JKRHeap* previousHeap = mDoExt_setCurrentHeap(g_iceKeeseHeap);
        g_iceKeeseModel = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000084);
        mDoExt_setCurrentHeap(previousHeap);
        if (g_iceKeeseModel == nullptr) {
            mDoExt_destroyExpHeap(g_iceKeeseHeap);
            g_iceKeeseHeap = nullptr;
            svc_log->error(mod_ctx, "Failed to create the Ice Keese model");
            g_iceKeeseLoadFailed = true;
            return;
        }

        svc_log->info(mod_ctx, "Ice Keese arrow-tip model loaded");
    } else if (phase == cPhs_ERROR_e || phase == cPhs_UNK3_e) {
        svc_log->error(mod_ctx, "Failed to load the Ice Keese model archive");
        g_iceKeeseLoadFailed = true;
    }
}

const char* secondaryItemNameForSlot(u8 slotId) {
    if (slotId == kShieldSurfSlot) {
        return "Shield Surf";
    }
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
        if (item == kIceArrowItem) {
            return "Ice Arrow (Bow)";
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
    return slotId == dItemNo_NONE_e || slotId == kShieldSurfSlot ||
           slotId == kSitSlot || slotId == kGoronTunicSlot ||
           (slotId >= kSecondarySlotBase &&
            slotId < kSecondarySlotBase + MAX_ITEM_SLOTS);
}
