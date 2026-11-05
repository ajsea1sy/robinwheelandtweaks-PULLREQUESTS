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

void clear_shield_surf_wheel_textures() {
    for (auto*& picture : g_shieldSurfWheelItemTex) {
        if (picture != nullptr) {
            JKR_DELETE(picture);
            picture = nullptr;
        }
    }
    if (g_shieldSurfWheelItemHeap != nullptr) {
        for (auto*& buffer : g_shieldSurfWheelItemBuf) {
            if (buffer != nullptr) {
                g_shieldSurfWheelItemHeap->free(buffer);
                buffer = nullptr;
            }
        }
    }
    g_shieldSurfWheelItemHeap = nullptr;
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

void load_shield_surf_wheel_textures(dMenu_Ring_c* ring) {
    clear_shield_surf_wheel_textures();
    g_shieldSurfWheelItemHeap = ring->mpHeap;
    for (auto*& buffer : g_shieldSurfWheelItemBuf) {
        buffer = static_cast<ResTIMG*>(ring->mpHeap->alloc(kItemTextureBufferSize, 0x20));
        if (buffer == nullptr) {
            svc_log->error(mod_ctx, "failed to allocate the Shield Surf wheel icon");
            clear_shield_surf_wheel_textures();
            return;
        }
    }

    const u8 shieldItem = shield_surf_display_item();
    const int textureCount = dMeter2Info_readItemTexture(
        shieldItem, g_shieldSurfWheelItemBuf[0], nullptr, g_shieldSurfWheelItemBuf[1],
        nullptr, g_shieldSurfWheelItemBuf[2], nullptr, nullptr, nullptr, -1);
    if (textureCount <= 0) {
        svc_log->error(mod_ctx, "failed to read the Shield Surf wheel icon");
        clear_shield_surf_wheel_textures();
        return;
    }
    for (int texture = 0; texture < std::min(textureCount, 3); ++texture) {
        g_shieldSurfWheelItemTex[texture] =
            JKR_NEW J2DPicture(g_shieldSurfWheelItemBuf[texture]);
        if (g_shieldSurfWheelItemTex[texture] == nullptr) {
            svc_log->error(mod_ctx, "failed to create the Shield Surf wheel icon");
            clear_shield_surf_wheel_textures();
            return;
        }
        g_shieldSurfWheelItemTex[texture]->setBasePosition(J2DBasePosition_4);
    }
    dMeter2Info_setItemColor(
        shieldItem, g_shieldSurfWheelItemTex[0], g_shieldSurfWheelItemTex[1],
        g_shieldSurfWheelItemTex[2], nullptr);
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

void record_secondary_ring_pose(dMenu_Ring_c* ring);

void setup_secondary_ring(dMenu_Ring_c* ring) {
    g_secondaryWheelSlotIds.fill(dItemNo_NONE_e);
    for (u8 slot = 0; slot < MAX_ITEM_SLOTS; ++slot) {
        g_secondaryWheelSlotIds[slot] = kSecondarySlotBase + slot;
    }
    g_secondaryWheelSlotIds[MAX_ITEM_SLOTS] = kShieldSurfSlot;
    g_secondaryWheelSlotIds[MAX_ITEM_SLOTS + 1] = kSitSlot;
    g_secondaryWheelSlotIds[MAX_ITEM_SLOTS + 2] = kGoronTunicSlot;

    ring->mItemsTotal = MAX_ITEM_SLOTS;
    ring->mTotalItemTexToAlloc = MAX_ITEM_SLOTS;
    ring->field_0x634 = 0x10000 / kSecondaryWheelSlotCount;
    ring->mCurrentSlot = 0;
    ring->mXButtonSlot = dItemNo_NONE_e;
    ring->mYButtonSlot = dItemNo_NONE_e;
    ring->mStatus = dMenu_Ring_c::STATUS_WAIT;
    ring->mOldStatus = dMenu_Ring_c::STATUS_WAIT;
    ring->mWaitFrames = std::max<s16>(g_ringHIO.mCursorChangeWaitFrames, 0);
    ring->field_0x674[0] = 0;
    ring->field_0x674[1] = 0;
    ring->field_0x674[2] = 0;
    ring->field_0x674[3] = 0;
    ring->mRingRadiusH = g_ringHIO.mRingRadiusH;
    ring->mRingRadiusV = g_ringHIO.mRingRadiusV;
    ring->mAlphaRate = 0.0f;
    ring->mCenterPosX = 0.0f;
    ring->mCenterPosY = 0.0f;
    if (ring->mRingOrigin == 0) {
        ring->mCenterPosY = FB_HEIGHT_BASE;
    } else if (ring->mRingOrigin == 2) {
        ring->mCenterPosY = -FB_HEIGHT_BASE;
    } else if (ring->mRingOrigin == 3) {
        ring->mCenterPosX = FB_WIDTH_BASE;
    } else {
        ring->mCenterPosX = -FB_WIDTH_BASE;
    }

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
    load_shield_surf_wheel_textures(ring);
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
    record_secondary_ring_pose(ring);
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

struct SecondaryRingPose {
    f32 centerX;
    f32 centerY;
    f32 alpha;
};

void record_secondary_ring_pose(dMenu_Ring_c* ring) {
#if TARGET_PC
    ring->captureRenderState();
#endif

    InterpMtx pose{};
    pose[0][0] = 1.0f;
    pose[1][1] = 1.0f;
    pose[2][2] = 1.0f;
    pose[0][3] = ring->mCenterPosX;
    pose[1][3] = ring->mCenterPosY;
    pose[2][3] = ring->mAlphaRate;
    if (svc_interp->record_mtx_keyed(mod_ctx, pose, ring) != MOD_OK &&
        !g_secondaryRingInterpolationErrorLogged)
    {
        g_secondaryRingInterpolationErrorLogged = true;
        svc_log->error(mod_ctx, "failed to record secondary wheel interpolation pose");
    }
}

SecondaryRingPose read_secondary_ring_pose(const dMenu_Ring_c* ring) {
    InterpMtx pose{};
    if (svc_interp->lookup_replacement_mtx(ring, pose)) {
        return {pose[0][3], pose[1][3], pose[2][3]};
    }
    return {ring->mCenterPosX, ring->mCenterPosY, ring->mAlphaRate};
}

void forget_secondary_ring_pose(const dMenu_Ring_c* ring) {
    if (ring != nullptr && svc_interp->forget_mtx(mod_ctx, ring) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to release secondary wheel interpolation pose");
    }
}

void position_secondary_cursor(dMenu_Ring_c* ring);

bool is_ice_arrow_slot(const dMenu_Ring_c* ring) {
    if (ring == nullptr || ring->mCurrentSlot >= kSecondaryWheelSlotCount) {
        return false;
    }

    const u8 selected = g_secondaryWheelSlotIds[ring->mCurrentSlot];
    return selected >= kSecondarySlotBase &&
           selected < kSecondarySlotBase + MAX_ITEM_SLOTS &&
           g_secondarySlots[selected - kSecondarySlotBase] == kIceArrowItem;
}

bool can_use_ice_arrow_combo(const dMenu_Ring_c* ring) {
    if (ring == nullptr || ring->mPlayerIsWolf) {
        return false;
    }

    for (int button = 0; button < 2; ++button) {
        const u8 mixItem = dComIfGs_getMixItemIndex(button);
        if ((dComIfGs_getSelectItemIndex(button) == SLOT_4 &&
             mixItem == dItemNo_NONE_e) ||
            mixItem == SLOT_4)
        {
            return true;
        }
    }
    return false;
}

void set_ice_arrow_selected(daAlink_c* player, bool selected) {
    if (player == nullptr || g_iceArrowSelected == selected) {
        return;
    }

    g_iceArrowSelected = selected;
    if (player->mItemAcKeep.getActor() != nullptr) {
        player->deleteArrow();
        player->makeArrow();
        player->setBowReloadAnime();
    }
}

void move_secondary_ring(dMenu_Ring_c* ring) {
    if (ring->mStatus == dMenu_Ring_c::STATUS_WAIT) {
        if (ring->mWaitFrames > 0) {
            --ring->mWaitFrames;
        } else if (ring->mpStick != nullptr &&
                   ring->mpStick->getValueStick() >= 0.75f)
        {
            const u8 targetSlot =
                closest_secondary_wheel_slot(ring->mpStick->getAngleStick());
            if (targetSlot != ring->mCurrentSlot) {
                const s16 angleDelta = static_cast<s16>(
                    g_secondaryWheelSlotAngles[targetSlot] -
                    g_secondaryWheelSlotAngles[ring->mCurrentSlot]);
                ring->mCurrentSlot = angleDelta < 0
                    ? (ring->mCurrentSlot + 1) % kSecondaryWheelSlotCount
                    : (ring->mCurrentSlot + kSecondaryWheelSlotCount - 1) %
                        kSecondaryWheelSlotCount;
                ring->field_0x670 = g_secondaryWheelSlotAngles[ring->mCurrentSlot];
                ring->mWaitFrames =
                    std::max<s16>(g_ringHIO.mCursorChangeWaitFrames, 0);
            }
        }
    }

    if (ring->mCurrentSlot < kSecondaryWheelSlotCount) {
        const u8 selected = g_secondaryWheelSlotIds[ring->mCurrentSlot];
        const bool iceArrowSlot = is_ice_arrow_slot(ring);
        auto* player = daAlink_getAlinkActorClass();
        if (ring->mStatus == dMenu_Ring_c::STATUS_WAIT && iceArrowSlot &&
            mDoCPd_c::getTrigR(PAD_1) != 0 &&
            can_use_ice_arrow_combo(ring) && player != nullptr &&
            !player->checkCanoeSlider())
        {
            const bool selectingIceArrow = !g_iceArrowSelected;
            set_ice_arrow_selected(player, selectingIceArrow);
            Z2GetAudioMgr()->seStart(
                selectingIceArrow ? Z2SE_SY_ITEM_COMBINE_ON : Z2SE_SY_ITEM_COMBINE_OFF,
                NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }
        if (!iceArrowSlot && mDoCPd_c::getTrigX(PAD_1) != 0) {
            g_secondarySelectedItems[0] = selected;
            ring->mXButtonSlot = ring->mCurrentSlot;
            ring->setSelectItem(0, secondaryDisplayItemForSlot(selected));
        }
        if (!iceArrowSlot && mDoCPd_c::getTrigY(PAD_1) != 0) {
            g_secondarySelectedItems[1] = selected;
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
    ring->setMixMessage();
    record_secondary_ring_pose(ring);
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
    record_secondary_ring_pose(ring);
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
        record_secondary_ring_pose(ring);
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
    record_secondary_ring_pose(ring);
    *static_cast<bool*>(retval) = closed;
    return HOOK_SKIP_ORIGINAL;
}

void draw_secondary_ring(dMenu_Ring_c* ring) {
    const SecondaryRingPose pose = read_secondary_ring_pose(ring);
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
    ring->mpSpotParent->setAlphaRate(pose.alpha * g_ringHIO.mOverlayAlpha);
    ring->mpSpotScreen->draw(0.0f, 0.0f, grafPort);
    ring->mpCenterParent->setAlphaRate(pose.alpha);
    ring->mpCenterScreen->draw(pose.centerX, pose.centerY, grafPort);
    ring->mpMessageParent->setAlphaRate(pose.alpha);
    ring->mpScreen->draw(pose.centerX, pose.centerY, grafPort);
    if (ring->mPikariFlashingSpeed > 0.0f) {
        CPaneMgr paneMgr;
        auto* buttonPane = ring->mpScreen->search(MULTI_CHAR('gr_btn'));
        if (buttonPane != nullptr) {
            const Vec pos = paneMgr.getGlobalVtxCenter(buttonPane, true, 0);
            dMeter2Info_getMeterClass()->getMeterDrawPtr()->drawPikari(
                pos.x + pose.centerX, pos.y + pose.centerY,
                &ring->mPikariFlashingSpeed, g_ringHIO.mPikariScale,
                g_ringHIO.mPikariFrontOuter, g_ringHIO.mPikariFrontInner,
                g_ringHIO.mPikariBackOuter, g_ringHIO.mPikariBackInner,
                g_ringHIO.mPikariAnimSpeed, 2);
        }
    }

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
        const f32 x = g_secondaryWheelSlotPosX[index] - 24.0f + pose.centerX;
        const f32 y = g_secondaryWheelSlotPosY[index] - 24.0f + pose.centerY;
        if (index == MAX_ITEM_SLOTS) {
            if (g_shieldSurfWheelItemBuf[0] != nullptr) {
                const u8 shieldItem = shield_surf_display_item();
                const f32 scale =
                    dItem_data::getTexScale(shieldItem) / 100.0f * kSecondaryIconScale;
                const f32 width = g_shieldSurfWheelItemBuf[0]->width / 48.0f * scale * 48.0f;
                const f32 height = g_shieldSurfWheelItemBuf[0]->height / 48.0f * scale * 48.0f;
                for (auto* picture : g_shieldSurfWheelItemTex) {
                    if (picture == nullptr) {
                        continue;
                    }
                    picture->setAlpha(pose.alpha * 255.0f);
                    picture->draw(x + (48.0f - width) * 0.5f,
                        y + (48.0f - height) * 0.5f, width, height, 0, 0, 0);
                }
            }
            continue;
        }
        if (index == MAX_ITEM_SLOTS + 1) {
            if (g_sitWheelItemTex != nullptr) {
                constexpr f32 iconSize = 36.0f;
                g_sitWheelItemTex->setAlpha(pose.alpha * 255.0f);
                g_sitWheelItemTex->draw(
                    x + (48.0f - iconSize) * 0.5f,
                    y + (48.0f - iconSize) * 0.5f,
                    iconSize, iconSize, 0, 0, 0);
            }
            continue;
        }
        if (index == MAX_ITEM_SLOTS + 2) {
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
                picture->setAlpha(pose.alpha * 255.0f);
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
            picture->setAlpha(pose.alpha * 255.0f);
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
            picture->setAlpha(pose.alpha * 255.0f);
            const f32 width = ring->field_0x548[button] * 48.0f;
            const f32 height = ring->field_0x558[button] * 48.0f;
            picture->draw(center.x - width * 0.5f + pose.centerX,
                center.y - height * 0.5f + pose.centerY,
                width, height, 0, 0, 0);
        }
    }

    if (ring->mCurrentSlot < kSecondaryWheelSlotCount) {
        ring->mpDrawCursor->setPos(
            g_secondaryWheelSlotPosX[ring->mCurrentSlot] + pose.centerX,
            g_secondaryWheelSlotPosY[ring->mCurrentSlot] + pose.centerY);
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
        forget_secondary_ring_pose(window->mpMenuRing);
        clear_shield_surf_wheel_textures();
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

HookAction show_ice_arrow_combo_on_prompt(ModContext*, void* args, void* retval, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!isSecondaryRing(ring)) {
        return HOOK_CONTINUE;
    }

    *static_cast<bool*>(retval) =
        ring->mStatus == dMenu_Ring_c::STATUS_WAIT && is_ice_arrow_slot(ring) &&
        !g_iceArrowSelected && can_use_ice_arrow_combo(ring);
    return HOOK_SKIP_ORIGINAL;
}

HookAction show_ice_arrow_combo_off_prompt(ModContext*, void* args, void* retval, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!isSecondaryRing(ring)) {
        return HOOK_CONTINUE;
    }

    *static_cast<bool*>(retval) =
        ring->mStatus == dMenu_Ring_c::STATUS_WAIT && is_ice_arrow_slot(ring) &&
        g_iceArrowSelected && can_use_ice_arrow_combo(ring);
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
         g_secondaryIconContext != SecondaryIconContext::DarkLink &&
         g_secondaryIconContext != SecondaryIconContext::IceArrow) ||
        mods::arg<u8>(args, 0) != dItemNo_WOOD_STICK_e)
    {
        return;
    }

    const char* resourceName =
        g_secondaryIconContext == SecondaryIconContext::Sit
            ? kSitIconResource
            : (g_secondaryIconContext == SecondaryIconContext::IceArrow
                   ? kIceArrowIconResource
                   : (g_secondaryIconContext == SecondaryIconContext::DarkLink
                          ? kDarkLinkIconResource
                          : kHoverBootsIconResource));
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
    auto* alink = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    if (button < 0 || button > 1 || !secondaryItemAssigned(button) ||
        (alink != nullptr && alink->checkWolf()))
    {
        g_secondaryIconContext = SecondaryIconContext::None;
        return HOOK_CONTINUE;
    }

    const u8 slot = g_secondarySelectedItems[button];
    g_secondaryIconContext = secondaryIconContextForSlot(slot);
    if (g_secondaryIconContext != SecondaryIconContext::Sit &&
        g_secondaryIconContext != SecondaryIconContext::HoverBoots &&
        g_secondaryIconContext != SecondaryIconContext::DarkLink &&
        g_secondaryIconContext != SecondaryIconContext::IceArrow)
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
            : (g_secondaryIconContext == SecondaryIconContext::IceArrow
                   ? kIceArrowIconResource
                   : (g_secondaryIconContext == SecondaryIconContext::DarkLink
                          ? kDarkLinkIconResource
                          : kHoverBootsIconResource));
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
