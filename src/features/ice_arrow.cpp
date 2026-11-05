void clear_ice_arrow_target(IceArrowFrozenTarget& target, fopAc_ac_c* actor) {
    if (target.shaderColorCaptured && actor != nullptr &&
        target.actorId == fopAcM_GetID(actor))
    {
        actor->tevStr.TevColor.r = target.originalTevColor.r;
        actor->tevStr.TevColor.g = target.originalTevColor.g;
        actor->tevStr.TevColor.b = target.originalTevColor.b;
    }

    if (actor != nullptr && target.actorId == fopAcM_GetID(actor)) {
        actor->gravity = target.originalGravity;
        actor->maxFallSpeed = target.originalMaxFallSpeed;
    }

    target.active = false;
    target.shaderColorCaptured = false;
    target.model = nullptr;
    target.framesRemaining = 0;
}

bool is_ice_arrow_enemy_dead(fopAc_ac_c* actor) {
    if (actor == nullptr || fopAcM_GetGroup(actor) != fopAc_ENEMY_e) {
        return true;
    }
    const auto* enemy = static_cast<fopEn_enemy_c*>(actor);
    return actor->health <= 0 || enemy->checkDeadFlg();
}

void register_ice_arrow_target(fopAc_ac_c* actor) {
    if (actor == nullptr || fopAcM_GetGroup(actor) != fopAc_ENEMY_e ||
        is_ice_arrow_enemy_dead(actor))
    {
        return;
    }

    IceArrowFrozenTarget* target = nullptr;
    for (auto& entry : g_iceArrowFrozenTargets) {
        if (entry.active && entry.actorId == fopAcM_GetID(actor)) {
            target = &entry;
            break;
        }
        if (!entry.active && target == nullptr) {
            target = &entry;
        }
    }

    if (target == nullptr) {
        target = &g_iceArrowFrozenTargets[0];
        for (auto& entry : g_iceArrowFrozenTargets) {
            if (entry.framesRemaining < target->framesRemaining) {
                target = &entry;
            }
        }
    }

    if (target->active && target->actorId != fopAcM_GetID(actor)) {
        fopAc_ac_c* replacedActor = fopAcM_SearchByID(target->actorId);
        clear_ice_arrow_target(*target, replacedActor);
    }

    if (!target->active) {
        target->actorId = fopAcM_GetID(actor);
        target->originalTevColor = actor->tevStr.TevColor;
        target->shaderColorCaptured = true;
        target->originalGravity = actor->gravity;
        target->originalMaxFallSpeed = actor->maxFallSpeed;
    }
    target->actorId = fopAcM_GetID(actor);
    target->model = actor->model;
    target->frozenPosition = actor->current.pos;
    target->frozenAngle = actor->current.angle;
    target->frozenShapeAngle = actor->shape_angle;
    target->framesRemaining = kIceArrowFreezeFrames;
    target->active = true;
    dComIfGp_particle_set(0, ID_AK_JN_ENEMYFREEZE00, &actor->current.pos, &actor->tevStr);
}

bool is_ice_arrow_actor_frozen(const fopAc_ac_c* actor) {
    if (actor == nullptr) {
        return false;
    }
    if (fopAcM_GetGroup(actor) != fopAc_ENEMY_e || actor->health <= 0 ||
        static_cast<const fopEn_enemy_c*>(actor)->checkDeadFlg())
    {
        return false;
    }

    const fpc_ProcID actorId = fopAcM_GetID(actor);
    for (const auto& target : g_iceArrowFrozenTargets) {
        if (target.active && target.actorId == actorId) {
            return true;
        }
    }
    return false;
}

bool release_dead_ice_arrow_actor(fpc_ProcID actorId) {
    for (auto& target : g_iceArrowFrozenTargets) {
        if (!target.active || target.actorId != actorId) {
            continue;
        }

        fopAc_ac_c* actor = fopAcM_SearchByID(actorId);
        if (!is_ice_arrow_enemy_dead(actor)) {
            return false;
        }

        clear_ice_arrow_target(target, actor);
        return true;
    }
    return false;
}

void hold_ice_arrow_target_in_place(ModContext*, void* args, void*, void*) {
    auto* leaf = mods::arg<leafdraw_class*>(args, 0);
    if (leaf == nullptr) {
        return;
    }

    auto* actor = reinterpret_cast<fopAc_ac_c*>(leaf);
    const fpc_ProcID actorId = fopAcM_GetID(actor);
    for (auto& target : g_iceArrowFrozenTargets) {
        if (!target.active || target.actorId != actorId) {
            continue;
        }

        if (is_ice_arrow_enemy_dead(actor)) {
            clear_ice_arrow_target(target, actor);
            return;
        }

        actor->current.pos = target.frozenPosition;
        actor->current.angle = target.frozenAngle;
        actor->old = actor->current;
        actor->shape_angle = target.frozenShapeAngle;
        actor->speed.set(0.0f, 0.0f, 0.0f);
        actor->speedF = 0.0f;
        actor->gravity = 0.0f;
        return;
    }
}

HookAction begin_ice_arrow_actor_execute(ModContext*, void* args, void*, void*) {
    auto* leaf = mods::arg<leafdraw_class*>(args, 0);
    if (leaf == nullptr ||
        g_iceArrowActorExecuteDepth >=
            sizeof(g_iceArrowPreviousExecutingActorIds) /
                sizeof(g_iceArrowPreviousExecutingActorIds[0]))
    {
        return HOOK_CONTINUE;
    }

    g_iceArrowPreviousExecutingActorIds[g_iceArrowActorExecuteDepth++] =
        g_iceArrowExecutingActorId;
    g_iceArrowExecutingActorId =
        fopAcM_GetID(reinterpret_cast<fopAc_ac_c*>(leaf));
    return HOOK_CONTINUE;
}

void end_ice_arrow_actor_execute(ModContext*, void*, void*, void*) {
    if (g_iceArrowActorExecuteDepth == 0) {
        return;
    }
    g_iceArrowExecutingActorId =
        g_iceArrowPreviousExecutingActorIds[--g_iceArrowActorExecuteDepth];
}

HookAction suppress_ice_frozen_enemy_attack(ModContext*, void* args, void*, void*) {
    auto* collider = mods::arg<cCcD_Obj*>(args, 0);
    if (collider == nullptr || !collider->ChkAtSet() ||
        !is_ice_arrow_actor_frozen(collider->GetAc()))
    {
        return HOOK_CONTINUE;
    }

    collider->OffAtSetBit();
    g_iceArrowSuppressedAttackCollider = collider;
    return HOOK_CONTINUE;
}

void restore_ice_frozen_enemy_attack(ModContext*, void* args, void*, void*) {
    auto* collider = mods::arg<cCcD_Obj*>(args, 0);
    if (collider != nullptr && collider == g_iceArrowSuppressedAttackCollider) {
        collider->OnAtSetBit();
        g_iceArrowSuppressedAttackCollider = nullptr;
    }
}

bool is_ice_arrow_target_model(J3DModel* model) {
    if (model == nullptr) {
        return false;
    }

    for (auto& target : g_iceArrowFrozenTargets) {
        if (!target.active || target.model != model) {
            continue;
        }

        fopAc_ac_c* actor = fopAcM_SearchByID(target.actorId);
        if (is_ice_arrow_enemy_dead(actor)) {
            clear_ice_arrow_target(target, actor);
            continue;
        }
        target.model = actor->model;
        return target.model == model;
    }

    return false;
}

HookAction update_ice_arrow_freezes(ModContext*, void* args, void*, void*) {
    if (mods::arg<daAlink_c*>(args, 0) == nullptr) {
        return HOOK_CONTINUE;
    }

    for (auto& target : g_iceArrowFrozenTargets) {
        if (!target.active) {
            continue;
        }

        fopAc_ac_c* actor = fopAcM_SearchByID(target.actorId);
        if (is_ice_arrow_enemy_dead(actor) || target.framesRemaining <= 1) {
            clear_ice_arrow_target(target, actor);
            continue;
        }

        target.model = actor->model;
        --target.framesRemaining;
    }
    return HOOK_CONTINUE;
}

HookAction apply_ice_arrow_bow_selection(ModContext*, void* args, void*, void*) {
    auto* player = mods::arg<daAlink_c*>(args, 0);
    if (g_secondaryRing != nullptr) {
        return HOOK_SKIP_ORIGINAL;
    }
    if (player == nullptr || !g_iceArrowSelected || player->checkCanoeSlider()) {
        return HOOK_CONTINUE;
    }

    const bool bowType =
        player->mEquipItem == dItemNo_BOW_e ||
        player->mEquipItem == dItemNo_BOMB_ARROW_e;
    if (!bowType) {
        return HOOK_CONTINUE;
    }

    player->setItemActionButtonStatus(BUTTON_STATUS_SWITCH);
    if (!player->arrowChangeTrigger()) {
        return HOOK_CONTINUE;
    }

    set_ice_arrow_selected(player, !g_iceArrowSelected);
    return HOOK_SKIP_ORIGINAL;
}

void set_ice_arrow_actor_type_after_creation(ModContext*, void* args, void*, void*) {
    auto* player = mods::arg<daAlink_c*>(args, 0);
    if (!g_iceArrowSelected || player == nullptr) {
        return;
    }

    fopAc_ac_c* actor = player->mItemAcKeep.getActor();
    if (actor != nullptr && fopAcM_GetName(actor) == fpcNm_ARROW_e) {
        static_cast<daArrow_c*>(actor)->mArrowType = kIceArrowType;
    }
}

void draw_ice_keese_on_held_arrow(ModContext*, void* args, void*, void*) {
    auto* arrow = mods::arg<daArrow_c*>(args, 0);
    if (!g_iceArrowSelected || g_iceKeeseModel == nullptr || arrow == nullptr ||
        arrow->mpModel == nullptr || arrow->mArrowType != kIceArrowType ||
        !arrow->checkWait() || arrow->field_0x93f != 0 || arrow->field_0x942 != 0)
    {
        return;
    }

    mDoMtx_stack_c::copy(arrow->mpModel->getBaseTRMtx());
    mDoMtx_stack_c::transM(0.0f, 0.0f, kIceKeeseArrowTipOffset);
    mDoMtx_stack_c::scaleM(kIceKeeseArrowScale, kIceKeeseArrowScale,
                           kIceKeeseArrowScale);
    g_iceKeeseModel->setBaseTRMtx(mDoMtx_stack_c::get());

    dKy_tevstr_c tevStr{};
    g_env_light.settingTevStruct(0, &arrow->current.pos, &tevStr);
    g_env_light.setLightTevColorType_MAJI(g_iceKeeseModel, &tevStr);
    mDoExt_modelUpdateDL(g_iceKeeseModel);
}

HookAction register_ice_arrow_hit(ModContext*, void* args, void*, void*) {
    auto* arrow = mods::arg<daArrow_c*>(args, 0);
    auto* actor = mods::arg<fopAc_ac_c*>(args, 2);
    auto* targetInfo = mods::arg<dCcD_GObjInf*>(args, 3);
    if (arrow != nullptr && arrow->mArrowType == kIceArrowType &&
        targetInfo != nullptr && !targetInfo->ChkTgShield() &&
        !targetInfo->ChkTgArrowThrough())
    {
        register_ice_arrow_target(actor);
    }
    return HOOK_CONTINUE;
}
HookAction freeze_ice_arrow_target_animation(ModContext*, void* args, void*, void*) {
    auto* morf = mods::arg<mDoExt_morf_c*>(args, 0);
    if (morf == nullptr) {
        return HOOK_CONTINUE;
    }

    if (release_dead_ice_arrow_actor(g_iceArrowExecutingActorId)) {
        return HOOK_CONTINUE;
    }

    bool actorIsFrozen = false;
    for (const auto& target : g_iceArrowFrozenTargets) {
        if (target.active && target.actorId == g_iceArrowExecutingActorId) {
            actorIsFrozen = true;
            break;
        }
    }

    return actorIsFrozen || is_ice_arrow_target_model(morf->mpModel)
               ? HOOK_SKIP_ORIGINAL
               : HOOK_CONTINUE;
}

void apply_ice_arrow_shader_after_tev_setup(ModContext*, void* args, void*, void*) {
    auto* tevStr = mods::arg<dKy_tevstr_c*>(args, 3);
    if (tevStr == nullptr) {
        return;
    }

    for (const auto& target : g_iceArrowFrozenTargets) {
        if (!target.active) {
            continue;
        }

        fopAc_ac_c* actor = fopAcM_SearchByID(target.actorId);
        if (actor == nullptr || &actor->tevStr != tevStr) {
            continue;
        }

        daAlink_c* player = daAlink_getAlinkActorClass();
        if (player != nullptr) {
            tevStr->TevColor.r = player->getFreezeR();
            tevStr->TevColor.g = player->getFreezeG();
            tevStr->TevColor.b = player->getFreezeB();
        }
        return;
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
DEFINE_HOOK(&dMenu_Ring_c::isMixItemOn, RingIsMixItemOn);
DEFINE_HOOK(&dMenu_Ring_c::isMixItemOff, RingIsMixItemOff);
DEFINE_HOOK(&dMeter2Draw_c::changeTextureItemXY, ChangeTextureItemXY);
DEFINE_HOOK(&dMeter2_c::moveButtonXY, MeterMoveButtonXY);
DEFINE_HOOK(&dMeter2Info_readItemTexture, ReadItemTexture);
DEFINE_HOOK(&daAlink_c::setWaterY, SetWaterY);
DEFINE_HOOK(&daAlink_c::setRoomInfo, SetRoomInfo);
DEFINE_HOOK(&daAlink_c::setStickData, SetStickData);
DEFINE_HOOK(&daAlink_c::checkGrabRooster, CheckGrabRooster);
DEFINE_HOOK(&daAlink_c::boardCommon, BoardCommon);
DEFINE_HOOK(&daAlink_c::checkSnowCode, CheckSnowCode);
DEFINE_HOOK(&daAlink_c::checkShieldDraw, CheckShieldDraw);
DEFINE_HOOK(&daAlink_c::checkNextActionBow, CheckNextActionBow);
DEFINE_HOOK(&daAlink_c::checkNotItemSinkLimit, CheckNotItemSinkLimit);
DEFINE_HOOK(&daAlink_c::setBodyAngleXReadyAnime, SetBodyAngleXReadyAnime);
#if TARGET_PC
DEFINE_HOOK(&daAlink_c::checkAimContext, CheckAimContext);
#endif
DEFINE_HOOK(&daAlink_c::rideGetOff, RideGetOff);
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
DEFINE_HOOK(&daAlink_c::procAutoJump, ProcAutoJump);
DEFINE_HOOK(&daAlink_c::execute, PlayerExecute);
DEFINE_HOOK(&daObjIceLeaf_c::execute, IceLeafExecute);
DEFINE_HOOK(&daObjIceLeaf_c::draw, IceLeafDraw);
DEFINE_HOOK(&daAlink_c::draw, PlayerDraw);
DEFINE_HOOK(&daAlink_c::makeArrow, MakeArrow);
DEFINE_HOOK(&daAlink_c::changeArrowType, ChangeArrowType);
DEFINE_HOOK(&daAlink_c::modelCalc, ModelCalc);
DEFINE_HOOK(&daArrow_c::atHitCallBack, ArrowAtHitCallBack);
DEFINE_HOOK(&daArrow_c::draw, ArrowDraw);
DEFINE_HOOK(&daArrow_c::arrowShooting, ArrowShooting);
DEFINE_HOOK(&mDoExt_morf_c::frameUpdate, MorfFrameUpdate);
DEFINE_HOOK(&fpcLf_Execute, LeafExecute);
DEFINE_HOOK(&cCcS::Set, CollisionSet);
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
DEFINE_HOOK(&dScnKy_env_light_c::settingTevStruct, SettingTevStruct);
DEFINE_HOOK(&daAlink_c::setWaterDropColor, SetWaterDropColor);
DEFINE_HOOK(&daItem_c::_daItem_execute, ItemExecute);
