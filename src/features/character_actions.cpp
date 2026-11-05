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
    svc_log->info(mod_ctx, "Sit action entered");
}

void begin_stand_action(daAlink_c* alink) {
    if (alink == nullptr || g_sitActionState != SitActionState::Seated) {
        return;
    }

    restore_sit_shape_angles(alink);
    alink->mSinkShapeOffset = 0.0f;
    alink->setSingleAnimeBase(daAlink_c::ANM_WAIT);
    g_sitActionState = SitActionState::Inactive;
    svc_log->info(mod_ctx, "Sit action ended by player");
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
    const bool eventRunning = alink != nullptr && alink->checkEventRun();
    if (alink == nullptr || alink->checkWolf() || eventRunning ||
        alink->mProcID != daAlink_c::PROC_WAIT || (!grounded && !hovering))
    {
        char message[128];
        std::snprintf(message, sizeof(message),
                      "Sit action ended by game state: actor=%u event=%u proc=%d grounded=%u hovering=%u",
                      static_cast<u32>(alink != nullptr),
                      static_cast<u32>(eventRunning),
                      alink != nullptr ? static_cast<int>(alink->mProcID) : -1,
                      static_cast<u32>(grounded), static_cast<u32>(hovering));
        svc_log->warn(mod_ctx, message);
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
        svc_log->warn(mod_ctx, "Sit action ended: no valid ground height");
        return;
    }

    align_link_to_ground(alink);
    alink->mNormalSpeed = 0.0f;
    alink->speedF = 0.0f;
}
