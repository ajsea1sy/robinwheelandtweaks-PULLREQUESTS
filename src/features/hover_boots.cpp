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

void stop_manual_glide_aura() {
    for (auto& footEmitters : g_manualGlideAuraEmitters) {
        for (u32& emitterId : footEmitters) {
            if (emitterId != 0) {
                dComIfGp_particle_setStopContinue(emitterId);
                emitterId = 0;
            }
        }
    }
}

void update_manual_glide_aura(daAlink_c* alink) {
    if (!g_manualCuccoGlideActive || alink == nullptr ||
        alink->mpLinkModel == nullptr || alink->checkWolf() ||
        alink->checkPlayerNoDraw())
    {
        stop_manual_glide_aura();
        return;
    }

    static const u16 effectIds[] = {
        ID_ZI_J_HB_MAGNE_A,
        ID_ZI_J_HB_MAGNE_B,
        ID_ZI_J_HB_MAGNE_C,
    };
    const u16 footJoints[] = {alink->field_0x30bc, alink->field_0x30be};

    for (int foot = 0; foot < 2; ++foot) {
        MtxP footMatrix = alink->mpLinkModel->getAnmMtx(footJoints[foot]);
        if (footMatrix == nullptr) {
            for (u32& emitterId : g_manualGlideAuraEmitters[foot]) {
                if (emitterId != 0) {
                    dComIfGp_particle_setStopContinue(emitterId);
                    emitterId = 0;
                }
            }
            continue;
        }

        for (int effect = 0; effect < 3; ++effect) {
            u32& emitterId = g_manualGlideAuraEmitters[foot][effect];
            emitterId = dComIfGp_particle_set(
                emitterId, effectIds[effect], &alink->current.pos, &alink->tevStr);
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

void reset_secondary_items_for_save_change() {
    g_goronTunicEquipped = false;
    disable_hover_boots();
    g_secondaryHudAssignmentsInitialized = false;
    g_secondaryHudRefreshPending.fill(false);
}

void clear_secondary_items_for_new_save(ModContext*, uint32_t, void*) {
    g_secondarySelectedItems.fill(dItemNo_NONE_e);
    reset_secondary_items_for_save_change();
}

void restore_secondary_items_for_save(ModContext* ctx, uint32_t, void*) {
    std::array<u8, MAX_SELECT_ITEM> assignments{};
    assignments.fill(dItemNo_NONE_e);
    size_t size = assignments.size();
    const ModResult result = svc_save->get_blob(
        ctx, kSecondaryAssignmentsBlobName, assignments.data(), &size);
    if (result == MOD_OK) {
        if (size != assignments.size()) {
            svc_log->warn(ctx, "discarded invalid saved secondary item assignments");
            assignments.fill(dItemNo_NONE_e);
        } else {
            for (u8& slotId : assignments) {
                if (!isValidSecondaryAssignment(slotId)) {
                    svc_log->warn(ctx, "discarded invalid saved secondary item assignment");
                    slotId = dItemNo_NONE_e;
                }
            }
        }
    } else if (result != MOD_UNAVAILABLE) {
        svc_log->error(ctx, "failed to load saved secondary item assignments");
    }

    g_secondarySelectedItems = assignments;
    reset_secondary_items_for_save_change();
}

void persist_secondary_items_for_save(ModContext* ctx, uint32_t, void*) {
    const ModResult result = svc_save->set_blob(
        ctx, kSecondaryAssignmentsBlobName, g_secondarySelectedItems.data(),
        g_secondarySelectedItems.size());
    if (result != MOD_OK) {
        svc_log->error(ctx, "failed to save secondary item assignments with the game save");
    }
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
