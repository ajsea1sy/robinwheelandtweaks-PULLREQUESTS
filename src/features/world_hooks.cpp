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
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    update_hover_magic_effect(alink);
    update_manual_glide_aura(alink);
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
