void delete_shield_surf_board() {
    if (g_shieldSurfBoardId != fpcM_ERROR_PROCESS_ID_e) {
        auto* board = static_cast<daObjIceLeaf_c*>(
            fopAcM_SearchByID(g_shieldSurfBoardId));
        if (board != nullptr) {
            board->setMode(daObjIceLeaf_c::MODE_RIDE_e);
            board->setAction(daObjIceLeaf_c::ACT_DEAD_e);
            board->offRide();
        }
        fopAcM_delete(g_shieldSurfBoardId);
        g_shieldSurfBoardId = fpcM_ERROR_PROCESS_ID_e;
    }
}

bool is_shield_surf_board_process(int processId) {
    return processId >= daAlink_c::PROC_BOARD_RIDE &&
           processId <= daAlink_c::PROC_BOARD_CUT_TURN;
}

void release_shield_surf_player_state(daAlink_c* alink) {
    if (alink == nullptr) {
        return;
    }

    fopAc_ac_c* rideActor = alink->getRideActor();
    if (rideActor != nullptr &&
        fopAcM_GetID(rideActor) == g_shieldSurfBoardId)
    {
        auto* board = static_cast<daObjIceLeaf_c*>(rideActor);
        board->setMode(daObjIceLeaf_c::MODE_RIDE_e);
        board->setAction(daObjIceLeaf_c::ACT_DEAD_e);
        board->offRide();
    }
    alink->mRideAcKeep.clearData();
    alink->mRideStatus = 0;
    alink->offModeFlg(daAlink_c::MODE_RIDING);
    alink->offNoResetFlg1(static_cast<daPy_py_c::daPy_FLG1>(0x1800));
    alink->attention_info.field_0xa = 10;
    alink->shape_angle.x = 0;
    alink->shape_angle.z = 0;
    delete_shield_surf_board();
}

void request_shield_surf_toggle(daAlink_c* alink) {
    if (g_shieldSurfActive) {
        g_shieldSurfDismountPending = true;
        return;
    }
    if (g_shieldSurfStartPending) {
        g_shieldSurfStartPending = false;
        g_shieldSurfStartWaitFrames = 0;
        delete_shield_surf_board();
        svc_log->info(mod_ctx, "Shield Surf start canceled");
        return;
    }
    if (alink == nullptr || alink->checkWolf() || alink->checkEventRun() ||
        alink->checkRideOn() || !alink->mLinkAcch.ChkGroundHit())
    {
        svc_log->warn(mod_ctx, "Shield Surf requires Link on walkable ground and not riding");
        return;
    }
    if (dComIfGs_getSelectEquipShield() == dItemNo_NONE_e ||
        alink->mShieldModel == nullptr)
    {
        svc_log->warn(mod_ctx, "Shield Surf requires an equipped shield");
        return;
    }

    g_shieldSurfStartPending = true;
    g_shieldSurfStartWaitFrames = 0;
    svc_log->info(mod_ctx, "Shield Surf start requested");
}

void process_shield_surf_transition(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr) {
        return;
    }

    if (g_shieldSurfActive) {
        if (!is_shield_surf_board_process(alink->mProcID)) {
            release_shield_surf_player_state(alink);
            g_shieldSurfActive = false;
            g_shieldSurfDismountPending = false;
            delete_shield_surf_board();
            svc_log->info(mod_ctx, "Shield Surf ended when Link left the board process");
            return;
        }
        if (g_shieldSurfDismountPending) {
            g_shieldSurfDismountPending = false;
            if (alink->checkBoardRide()) {
                alink->setJumpMode();
                alink->procBackJumpInit(0);
            }
        }
        if (!alink->checkBoardRide()) {
            g_shieldSurfActive = false;
            delete_shield_surf_board();
            svc_log->info(mod_ctx, "Shield Surf ended");
        }
        return;
    }
    if (!g_shieldSurfStartPending) {
        return;
    }
    if (alink->checkWolf() || alink->checkEventRun() || alink->checkRideOn() ||
        !alink->mLinkAcch.ChkGroundHit() ||
        dComIfGs_getSelectEquipShield() == dItemNo_NONE_e ||
        alink->mShieldModel == nullptr)
    {
        g_shieldSurfStartPending = false;
        delete_shield_surf_board();
        svc_log->warn(mod_ctx, "Shield Surf start canceled because Link is no longer ready");
        return;
    }

    if (g_shieldSurfBoardId == fpcM_ERROR_PROCESS_ID_e) {
        cXyz spawnPosition = alink->current.pos;
        g_shieldSurfBoardId = fopAcM_create(
            fpcNm_Obj_IceLeaf_e, 0x100FF, &spawnPosition,
            fopAcM_GetRoomNo(alink), &alink->shape_angle, nullptr, -1);
        if (g_shieldSurfBoardId == fpcM_ERROR_PROCESS_ID_e) {
            g_shieldSurfStartPending = false;
            svc_log->error(mod_ctx, "Failed to create the native Ice Leaf board actor");
            return;
        }
        return;
    }

    auto* board = static_cast<daObjIceLeaf_c*>(
        fopAcM_SearchByID(g_shieldSurfBoardId));
    if (board != nullptr) {
        board->setMode(daObjIceLeaf_c::MODE_RIDE_e);
        board->setAction(daObjIceLeaf_c::ACT_DEAD_e);
        board->current.pos = alink->current.pos;
        board->shape_angle = alink->shape_angle;
    }
    if (board == nullptr || board->mpModel == nullptr) {
        if (++g_shieldSurfStartWaitFrames >= 300) {
            g_shieldSurfStartPending = false;
            delete_shield_surf_board();
            svc_log->error(mod_ctx, "Timed out loading the native Shield Surf board actor");
        }
        return;
    }

    board->setMode(daObjIceLeaf_c::MODE_RIDE_e);
    board->setAction(daObjIceLeaf_c::ACT_DEAD_e);
    if (!alink->procBoardWaitInit(board) || !alink->checkBoardRide()) {
        g_shieldSurfStartPending = false;
        delete_shield_surf_board();
        svc_log->warn(mod_ctx, "The game rejected the Shield Surf board process");
        return;
    }

    board->setBaseMtx();
    g_shieldSurfStartPending = false;
    g_shieldSurfActive = true;
    svc_log->info(mod_ctx, "Shield Surf started using Link's equipped shield");
}
HookAction allow_shield_surf_on_any_ground(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (g_shieldSurfActive && alink != nullptr && alink->checkBoardRide()) {
        *static_cast<bool*>(retval) = true;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction suppress_player_shield_draw_during_surf(
    ModContext*, void* args, void* retval, void*)
{
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (g_shieldSurfActive && alink != nullptr && alink->checkBoardRide()) {
        *static_cast<bool*>(retval) = false;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction suppress_bow_action_during_shield_surf(
    ModContext*, void* args, void* retval, void*)
{
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (g_shieldSurfActive && alink != nullptr && alink->checkBoardRide()) {
        *static_cast<int*>(retval) = 0;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction release_shield_surf_ride_safely(
    ModContext*, void* args, void*, void*)
{
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!g_shieldSurfActive || alink == nullptr || !alink->checkBoardRide()) {
        return HOOK_CONTINUE;
    }

    release_shield_surf_player_state(alink);
    return HOOK_SKIP_ORIGINAL;
}

HookAction keep_shield_surf_board_in_ride_mode(
    ModContext*, void* args, void*, void*)
{
    auto* board = mods::arg<daObjIceLeaf_c*>(args, 0);
    if (board == nullptr || fopAcM_GetID(board) != g_shieldSurfBoardId) {
        return HOOK_CONTINUE;
    }

    board->setMode(daObjIceLeaf_c::MODE_RIDE_e);
    board->setAction(daObjIceLeaf_c::ACT_DEAD_e);
    return HOOK_CONTINUE;
}

void boost_shield_surf_speed(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    const BOOL canMove = mods::arg<BOOL>(args, 1);
    if (!g_shieldSurfActive || !shieldSurfRBoostEnabled() || !canMove ||
        alink == nullptr || !alink->checkBoardRide() ||
        mDoCPd_c::getHoldR(PAD_1) == 0)
    {
        return;
    }

    cLib_chaseF(
        &alink->mNormalSpeed,
        alink->mMaxSpeed,
        daAlinkHIO_board_c0::m.mPushAccel);
}

HookAction draw_shield_surf_on_native_board(
    ModContext*, void* args, void* retval, void*)
{
    auto* board = mods::arg<daObjIceLeaf_c*>(args, 0);
    if (board == nullptr || fopAcM_GetID(board) != g_shieldSurfBoardId) {
        return HOOK_CONTINUE;
    }

    *static_cast<int*>(retval) = 1;
    if (!g_shieldSurfActive || board->mpModel == nullptr) {
        return HOOK_SKIP_ORIGINAL;
    }

    auto* alink = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    if (alink == nullptr || !alink->checkBoardRide() ||
        dComIfGs_getSelectEquipShield() == dItemNo_NONE_e ||
        alink->mShieldModel == nullptr)
    {
        return HOOK_SKIP_ORIGINAL;
    }

    J3DModel* shield = alink->mShieldModel;
    Mtx originalMatrix;
    MTXCopy(shield->getBaseTRMtx(), originalMatrix);
    mDoMtx_stack_c::copy(board->mpModel->getBaseTRMtx());
    mDoMtx_stack_c::XrotM(0x4000);
    mDoMtx_stack_c::ZrotM(0x8000);
    Mtx surfMatrix;
    MTXCopy(mDoMtx_stack_c::get(), surfMatrix);
    surfMatrix[1][3] += kShieldSurfBoardRaise;
    shield->setBaseTRMtx(surfMatrix);

    dKy_tevstr_c tevStr{};
    g_env_light.settingTevStruct(0, &board->current.pos, &tevStr);
    g_env_light.setLightTevColorType_MAJI(shield, &tevStr);
    mDoExt_modelUpdateDL(shield);

    shield->setBaseTRMtx(originalMatrix);
    return HOOK_SKIP_ORIGINAL;
}
