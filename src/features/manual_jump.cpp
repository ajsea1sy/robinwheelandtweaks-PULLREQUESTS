bool start_manual_cucco_glide(daAlink_c* alink) {
    if (alink->mProcID == daAlink_c::PROC_FALL) {
        if (!alink->procAutoJumpInit(1)) {
            return false;
        }
        alink->speed.y = 0.0f;
    } else if (alink->mProcID != daAlink_c::PROC_AUTO_JUMP) {
        return false;
    }

    auto* cucco = alink->mGrabItemAcKeep.getActor();
    const bool isGoldCucco =
        cucco != nullptr && fopAcM_GetName(cucco) == fpcNm_NI_e &&
        static_cast<ni_class*>(cucco)->checkGold();
    if (isGoldCucco) {
        alink->mMaxSpeed = 30.0f;
        alink->field_0x3478 = -10.0f;
    } else if (alink->checkStageName("F_SP115")) {
        alink->mMaxSpeed = 27.0f;
        alink->field_0x3478 = -10.0f;
    } else {
        alink->mMaxSpeed = alink->mpHIO->mAutoJump.m.mCuccoJumpMaxSpeed;
        alink->field_0x3478 = alink->mpHIO->mAutoJump.m.mCuccoFallMaxSpeed;
    }

    alink->mProcVar2.field_0x300c = 1;
    alink->field_0x30a0 = 0x1800;
    alink->setSpecialGravity(-1.0f, alink->field_0x3478, FALSE);
    return true;
}

void release_manual_cucco_glide(daAlink_c* alink) {
    g_manualCuccoGlideActive = false;
    alink->mProcVar2.field_0x300c = 0;
    alink->setSpecialGravity(alink->mpHIO->mAutoJump.m.mGravity,
                             alink->mpHIO->mAutoJump.m.mMaxFallSpeed, TRUE);
    alink->resetUpperAnime(daAlink_c::UPPER_1, 3.0f);
    alink->field_0x30a0 = 0;
    alink->onModeFlg(4);
}

HookAction release_manual_cucco_glide_on_a(
    ModContext*, void* args, void*, void*)
{
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (g_manualCuccoGlideActive && alink != nullptr &&
        alink == dComIfGp_getLinkPlayer() &&
        (alink->mItemTrigger & daAlink_c::BTN_A) != 0)
    {
        alink->mItemTrigger &= static_cast<u8>(~daAlink_c::BTN_A);
        release_manual_cucco_glide(alink);
    }
    return HOOK_CONTINUE;
}

HookAction emulate_manual_cucco_glide_check(
    ModContext*, void* args, void* retval, void*)
{
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (g_manualCuccoGlideActive && alink != nullptr &&
        alink == dComIfGp_getLinkPlayer() &&
        alink->mProcID == daAlink_c::PROC_AUTO_JUMP &&
        !alink->mLinkAcch.ChkGroundHit())
    {
        *static_cast<BOOL*>(retval) = TRUE;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

void update_manual_cucco_glide(daAlink_c* alink) {
    if (g_manualCuccoGlideActive &&
        (alink == nullptr || alink->checkEventRun() || alink->checkWolf() ||
         alink->mLinkAcch.ChkGroundHit() ||
         alink->mProcID != daAlink_c::PROC_AUTO_JUMP ||
         alink->mProcVar2.field_0x300c == 0))
    {
        g_manualCuccoGlideActive = false;
    }
}

void update_hover_plane_lowering(daAlink_c* alink) {
    const bool held = hoverPlaneLowerBindingHeld(hoverPlaneLowerBinding());
    const bool pressed = held && !g_hoverPlaneLowerBindingWasHeld;
    g_hoverPlaneLowerBindingWasHeld = held;
    if (!pressed || !hoverBootsAirwalkingEnabled() || !g_hoverBootsEquipped ||
        !g_hoverGroundHeightValid || alink == nullptr)
    {
        return;
    }

    if (alink->mLinkAcch.ChkGroundHit() || alink->checkEventRun() ||
        alink->checkModeFlg(daAlink_c::MODE_SWIMMING) ||
        alink->checkNoResetFlg0(daAlink_c::FLG0_WATER_IN_MOVE))
    {
        return;
    }

    const auto& jump = alink->mpHIO->mAutoJump.m;
    const f32 standingJumpSpeed =
        jump.mAlwaysMaxSpeedJump ? jump.mMaxJumpSpeed : jump.mMinJumpSpeed;
    const f32 verticalJumpSpeed =
        standingJumpSpeed * jump.mJumpSpeedRate * cM_ssin(jump.mJumpAngle);
    const f32 gravityMagnitude = std::abs(jump.mGravity);
    if (gravityMagnitude <= 0.0f || !std::isfinite(verticalJumpSpeed) ||
        verticalJumpSpeed <= 0.0f)
    {
        svc_log->warn(mod_ctx, "could not determine vanilla jump height for Hover Boots");
        return;
    }

    const f32 vanillaJumpHeight =
        verticalJumpSpeed * verticalJumpSpeed / (2.0f * gravityMagnitude);
    if (!std::isfinite(vanillaJumpHeight) || vanillaJumpHeight <= 0.0f) {
        svc_log->warn(mod_ctx, "could not determine vanilla jump height for Hover Boots");
        return;
    }

    if (g_hoverAtSurface) {
        alink->procFallInit(1, jump.mFallInterpolation);
    }
    alink->setSpecialGravity(jump.mGravity, jump.mMaxFallSpeed, TRUE);
    alink->speed.y = std::min(alink->speed.y, -0.5f);
    g_hoverGroundHeight -= vanillaJumpHeight;
    g_hoverLandingSequenceActive = false;
    g_hoverAtSurface = false;
    g_hoverLandingFrames = 0;
}

bool try_manual_jump(daAlink_c* alink) {
    if (alink == nullptr || g_secondaryRing != nullptr || g_primaryRingOpen ||
        g_sitActionState != SitActionState::Inactive || g_shieldSurfActive ||
        alink->checkWolf() || alink->checkEventRun() || alink->checkRideOn())
    {
        return false;
    }

    int buttonMask = 0;
    bool triggered = false;
    switch (manualJumpBinding()) {
    case 1:
        buttonMask = daAlink_c::BTN_A;
        triggered = mDoCPd_c::getTrigA(PAD_1) != 0;
        break;
    case 2:
        buttonMask = daAlink_c::BTN_B;
        triggered = mDoCPd_c::getTrigB(PAD_1) != 0;
        break;
    case 3:
        buttonMask = daAlink_c::BTN_L;
        triggered = mDoCPd_c::getTrigL(PAD_1) != 0;
        break;
    case 4:
        buttonMask = daAlink_c::BTN_R;
        triggered = mDoCPd_c::getTrigR(PAD_1) != 0;
        break;
    case 5:
        buttonMask = daAlink_c::BTN_Z;
        triggered = mDoCPd_c::getTrigZ(PAD_1) != 0;
        break;
    default:
        return false;
    }

    if (!triggered) {
        return false;
    }

    if (g_manualCuccoGlideActive) {
        if (buttonMask != daAlink_c::BTN_A) {
            alink->mItemTrigger &= static_cast<u8>(~buttonMask);
            release_manual_cucco_glide(alink);
        }
        return true;
    }

    const bool hovering = is_hover_boots_hovering(alink);
    if (!alink->mLinkAcch.ChkGroundHit() && !hovering) {
        if (g_hoverBootsEquipped) {
            if (!hoverBootsAirwalkingEnabled() ||
                alink->checkModeFlg(daAlink_c::MODE_SWIMMING) ||
                alink->checkNoResetFlg0(daAlink_c::FLG0_WATER_IN_MOVE))
            {
                return false;
            }

            g_hoverGroundHeight = alink->current.pos.y;
            g_hoverGroundHeightValid = true;
            g_hoverLandingSequenceActive = false;
            g_hoverAtSurface = false;
            g_hoverLandingFrames = 0;
            alink->mItemTrigger &= static_cast<u8>(~buttonMask);
            return true;
        }

        if (alink->mProcID != daAlink_c::PROC_FALL &&
            alink->mProcID != daAlink_c::PROC_AUTO_JUMP)
        {
            return false;
        }

        if (alink->checkGrabGlide()) {
            if (!start_manual_cucco_glide(alink)) {
                return false;
            }
        } else {
            g_manualCuccoGlideActive = true;
            if (!start_manual_cucco_glide(alink)) {
                g_manualCuccoGlideActive = false;
                return false;
            }
        }

        alink->mLinkAcch.ClrGroundHit();
        alink->mItemTrigger &= static_cast<u8>(~buttonMask);
        return true;
    }

    if (alink->mProcID != daAlink_c::PROC_WAIT &&
        alink->mProcID != daAlink_c::PROC_MOVE)
    {
        return false;
    }

    const f32 horizontalMomentum = alink->mNormalSpeed;
    const s16 momentumAngle = alink->current.angle.y;
    if (!alink->procAutoJumpInit(0)) {
        return false;
    }

    const f32 heightScale = std::sqrt(
        static_cast<f32>(manualJumpHeightPercent()) / 100.0f);
    alink->speed.y *= heightScale;
    alink->mNormalSpeed = horizontalMomentum;
    alink->current.angle.y = momentumAngle;
    if (hovering) {
        g_hoverLandingSequenceActive = false;
        g_hoverAtSurface = false;
        g_hoverLandingFrames = 0;
    }
    alink->mLinkAcch.ClrGroundHit();
    alink->mItemTrigger &= static_cast<u8>(~buttonMask);
    return true;
}

void update_secondary_assigned_item_input(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr) {
        return;
    }

    update_hover_plane_lowering(alink);

    if (alink->checkWolf()) {
        return;
    }

    if (try_manual_jump(alink)) {
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
    case kShieldSurfItemId:
        request_shield_surf_toggle(alink);
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
