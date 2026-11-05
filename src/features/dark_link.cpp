void restore_dark_link_colors() {
    g_darkLinkFormBlend = 0.0f;
    g_darkLinkDrawModelData = nullptr;
    g_darkLinkAppearanceModelData.fill(nullptr);
    g_darkLinkAppearanceModelDataCount = 0;
}

void play_dark_link_transition_effect(daAlink_c* alink, bool activating) {
    if (alink == nullptr) {
        return;
    }

    cXyz scale(1.0f, 1.0f, 1.0f);
    const u16 effectId = activating ? ID_ZI_S_LK_WARP_APP_A
                                    : ID_ZI_S_LK_WARP_DISAPP_A;
    dComIfGp_particle_set(dPa_RM(effectId), &alink->current.pos, nullptr, &scale);
}

f32 smoothstep(f32 edge0, f32 edge1, f32 value) {
    const f32 t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

bool contains_case_insensitive(const char* text, const char* needle) {
    if (text == nullptr || needle == nullptr || *needle == '\0') {
        return false;
    }

    for (const char* start = text; *start != '\0'; ++start) {
        const char* textChar = start;
        const char* needleChar = needle;
        while (*textChar != '\0' && *needleChar != '\0' &&
               std::tolower(static_cast<unsigned char>(*textChar)) ==
                   std::tolower(static_cast<unsigned char>(*needleChar)))
        {
            ++textChar;
            ++needleChar;
        }
        if (*needleChar == '\0') {
            return true;
        }
    }
    return false;
}

bool material_uses_pupil_texture(J3DModelData* modelData, J3DMaterial* material) {
    if (modelData == nullptr || material == nullptr) {
        return false;
    }

    J3DTexture* textures = modelData->getTexture();
    JUTNameTab* textureNames = modelData->getTextureName();
    J3DTevBlock* tevBlock = material->getTevBlock();
    if (textures == nullptr || textureNames == nullptr || tevBlock == nullptr) {
        return false;
    }

    const u8 stageCount = std::min<u8>(material->getTevStageNum(), 16);
    for (u8 stage = 0; stage < stageCount; ++stage) {
        J3DTevOrder* order = tevBlock->getTevOrder(stage);
        const u8 textureMap = order != nullptr ? order->getTexMap() : 0xFF;
        if (textureMap > 7) {
            continue;
        }

        const u16 textureIndex = tevBlock->getTexNo(textureMap);
        if (textureIndex >= textures->getNum()) {
            continue;
        }

        const char* textureName = textureNames->getName(textureIndex);
        if (contains_case_insensitive(textureName, "pupil") ||
            contains_case_insensitive(textureName, "iris") ||
            contains_case_insensitive(textureName, "hitomi"))
        {
            return true;
        }
    }
    return false;
}

void remember_dark_link_model_data(J3DModelData* modelData) {
    if (modelData == nullptr) {
        return;
    }

    for (u8 i = 0; i < g_darkLinkAppearanceModelDataCount; ++i) {
        if (g_darkLinkAppearanceModelData[i] == modelData) {
            return;
        }
    }
    if (g_darkLinkAppearanceModelDataCount < g_darkLinkAppearanceModelData.size()) {
        g_darkLinkAppearanceModelData[g_darkLinkAppearanceModelDataCount++] =
            modelData;
    }
}

void remember_dark_link_model_data(J3DModel* model) {
    remember_dark_link_model_data(
        model != nullptr ? model->getModelData() : nullptr);
}

bool is_dark_link_appearance_model_data(J3DModelData* modelData) {
    if (modelData == nullptr) {
        return false;
    }
    for (u8 i = 0; i < g_darkLinkAppearanceModelDataCount; ++i) {
        if (g_darkLinkAppearanceModelData[i] == modelData) {
            return true;
        }
    }
    return false;
}

HookAction mark_dark_link_model_for_draw(ModContext*, void* args, void*, void*) {
    g_darkLinkDrawModelData = nullptr;
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    auto* model = mods::arg<J3DModel*>(args, 1);
    if (alink == nullptr || model == nullptr) {
        return HOOK_CONTINUE;
    }

    g_darkLinkAppearanceModelData.fill(nullptr);
    g_darkLinkAppearanceModelDataCount = 0;
    if (!alink->checkWolf() && g_darkLinkFormBlend > 0.0f) {
        remember_dark_link_model_data(alink->mpLinkModel);
        remember_dark_link_model_data(alink->mpLinkFaceModel);
        remember_dark_link_model_data(alink->mpLinkHatModel);
        remember_dark_link_model_data(alink->mpLinkHandModel);
        remember_dark_link_model_data(alink->mpLinkBootModels[0]);
        remember_dark_link_model_data(alink->mpLinkBootModels[1]);
        remember_dark_link_model_data(alink->mShieldModel);
        remember_dark_link_model_data(alink->mSwordModel);
        remember_dark_link_model_data(alink->mSheathModel);
        remember_dark_link_model_data(alink->field_0x064C);
    }

    const bool isLinkAppearanceModel =
        model == alink->mpLinkModel || model == alink->mpLinkFaceModel ||
        model == alink->mpLinkHatModel || model == alink->mpLinkHandModel ||
        model == alink->mpLinkBootModels[0] || model == alink->mpLinkBootModels[1] ||
        model == alink->mShieldModel || model == alink->mSwordModel ||
        model == alink->mSheathModel ||
        (model->getModelData() != nullptr &&
         model->getModelData() == alink->field_0x064C);
    if (isLinkAppearanceModel) {
        if (!alink->checkWolf() && g_darkLinkFormBlend > 0.0f) {
            g_darkLinkDrawModelData = model->getModelData();
        }
    }
    return HOOK_CONTINUE;
}

void apply_dark_link_after_lighting(ModContext*, void* args, void*, void*) {
    auto* modelData = mods::arg<J3DModelData*>(args, 1);
    if (modelData == nullptr || modelData != g_darkLinkDrawModelData ||
        g_darkLinkFormBlend <= 0.0f)
    {
        return;
    }

    g_darkLinkDrawMaterialSnapshotCount = 0;
    auto* materialNames = modelData->getMaterialName();
    f32 modelMinY = G_CM3D_F_INF;
    f32 modelMaxY = -G_CM3D_F_INF;
    for (u16 i = 0; i < modelData->getMaterialNum(); ++i) {
        J3DMaterial* material = modelData->getMaterialNodePointer(i);
        J3DShape* shape = material != nullptr ? material->getShape() : nullptr;
        Vec* shapeMin = shape != nullptr ? shape->getMin() : nullptr;
        Vec* shapeMax = shape != nullptr ? shape->getMax() : nullptr;
        if (shapeMin == nullptr || shapeMax == nullptr) {
            continue;
        }
        modelMinY = std::min(modelMinY, shapeMin->y);
        modelMaxY = std::max(modelMaxY, shapeMax->y);
    }

    for (u16 i = 0; i < modelData->getMaterialNum(); ++i) {
        J3DMaterial* material = modelData->getMaterialNodePointer(i);
        if (material == nullptr) {
            continue;
        }

        J3DTevBlock* tevBlock = material->getTevBlock();
        const u8 stageCount = material->getTevStageNum();
        J3DTevStage* stage =
            tevBlock != nullptr && stageCount > 0
                ? tevBlock->getTevStage(stageCount - 1)
                : nullptr;
        J3DGXColorS10* tevColor = material->getTevColor(0);
        J3DGXColor* tevKColor = material->getTevKColor(0);
        if (g_darkLinkDrawMaterialSnapshotCount >=
            g_darkLinkDrawMaterialSnapshots.size())
        {
            svc_log->error(mod_ctx, "Dark Link Form exceeded its draw material capacity");
            break;
        }

        DarkLinkDrawMaterialSnapshot& snapshot =
            g_darkLinkDrawMaterialSnapshots[g_darkLinkDrawMaterialSnapshotCount++];
        snapshot.material = material;
        snapshot.stageIndex = stageCount > 0 ? stageCount - 1 : 0;
        if (stage != nullptr) {
            snapshot.tevKColorSel =
                tevBlock->getTevKColorSel(snapshot.stageIndex);
            snapshot.stage = *stage;
        }
        snapshot.hasTevColor = tevColor != nullptr;
        snapshot.hasTevKColor = tevKColor != nullptr;
        if (snapshot.hasTevColor) {
            snapshot.tevColor = *tevColor;
        }
        if (snapshot.hasTevKColor) {
            snapshot.tevKColor = *tevKColor;
        }

        const char* name = materialNames != nullptr ? materialNames->getName(i) : nullptr;
        snapshot.isPupilMaterial =
            contains_case_insensitive(name, "pupil") ||
            contains_case_insensitive(name, "iris") ||
            contains_case_insensitive(name, "hitomi") ||
            material_uses_pupil_texture(modelData, material);
        snapshot.blend = g_darkLinkFormBlend;
        J3DShape* shape = material->getShape();
        Vec* shapeMin = shape != nullptr ? shape->getMin() : nullptr;
        Vec* shapeMax = shape != nullptr ? shape->getMax() : nullptr;
        if (modelMaxY > modelMinY && shapeMin != nullptr && shapeMax != nullptr) {
            const f32 centerY = (shapeMin->y + shapeMax->y) * 0.5f;
            const f32 span = modelMaxY - modelMinY;
            const f32 band = std::max(12.0f, span * 0.04f);
            const f32 fragmentOffset =
                (static_cast<f32>((i * 37u + 11u) % 17u) / 16.0f - 0.5f) * band;
            const f32 progress =
                g_darkLinkFormActive ? g_darkLinkFormBlend : 1.0f - g_darkLinkFormBlend;
            if (g_darkLinkFormActive) {
                const f32 front = modelMaxY + band - progress * (span + 2.0f * band);
                snapshot.blend =
                    smoothstep(-band, band, centerY + fragmentOffset - front);
            } else {
                const f32 front = -band + progress * (span + 2.0f * band);
                snapshot.blend =
                    1.0f - smoothstep(
                               -band, band,
                               front - (centerY + fragmentOffset - modelMinY));
            }
        }
        if (g_darkLinkFormActive && g_darkLinkFormBlend >= 1.0f) {
            snapshot.blend = 1.0f;
        } else if (!g_darkLinkFormActive && g_darkLinkFormBlend <= 0.0f) {
            snapshot.blend = 0.0f;
        }
    }
}

void apply_dark_link_material_output(DarkLinkDrawMaterialSnapshot& snapshot) {
    J3DMaterial* material = snapshot.material;
    if (material == nullptr) {
        return;
    }

    J3DTevBlock* tevBlock = material->getTevBlock();
    J3DTevStage* stage =
        tevBlock != nullptr ? tevBlock->getTevStage(snapshot.stageIndex) : nullptr;
    if (stage == nullptr) {
        return;
    }

    if (snapshot.blend < 1.0f) {
        if (!snapshot.hasTevColor || !snapshot.hasTevKColor) {
            return;
        }

        J3DGXColorS10 factor{};
        const s16 brightness = static_cast<s16>((1.0f - snapshot.blend) * 255.0f);
        factor.r = brightness;
        factor.g = brightness;
        factor.b = brightness;
        factor.a = 255;
        J3DGXColor target{};
        target.r =
            static_cast<u8>(snapshot.isPupilMaterial ? 255.0f * snapshot.blend : 0.0f);
        target.g = 0;
        target.b = 0;
        target.a = 255;
        stage->setTevColorAB(GX_CC_CPREV, GX_CC_ZERO);
        stage->setTevColorCD(GX_CC_C0,
                             snapshot.isPupilMaterial ? GX_CC_KONST : GX_CC_ZERO);
        if (snapshot.isPupilMaterial) {
            tevBlock->setTevKColorSel(snapshot.stageIndex, GX_TEV_KCSEL_K0);
        }
        material->setTevColor(0, &factor);
        material->setTevKColor(0, &target);
    } else if (snapshot.isPupilMaterial &&
               (snapshot.hasTevKColor || snapshot.hasTevColor))
    {
        stage->setTevColorAB(GX_CC_ZERO, GX_CC_ZERO);
        if (snapshot.hasTevKColor) {
            stage->setTevColorCD(GX_CC_ZERO, GX_CC_KONST);
            tevBlock->setTevKColorSel(snapshot.stageIndex, GX_TEV_KCSEL_K0);
            J3DGXColor target{};
            target.r = 255;
            target.g = 0;
            target.b = 0;
            target.a = 255;
            material->setTevKColor(0, &target);
        } else {
            stage->setTevColorCD(GX_CC_ZERO, GX_CC_C0);
            J3DGXColorS10 target{};
            target.r = 255;
            target.g = 0;
            target.b = 0;
            target.a = 255;
            material->setTevColor(0, &target);
        }
    } else {
        stage->setTevColorAB(GX_CC_ZERO, GX_CC_ZERO);
        stage->setTevColorCD(GX_CC_ZERO, GX_CC_ZERO);
    }
    stage->setTevColorOp(
        GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}

HookAction apply_dark_link_material_before_display(ModContext*, void* args, void*, void*) {
    auto* material = mods::arg<J3DMaterial*>(args, 0);
    for (u16 i = 0; i < g_darkLinkDrawMaterialSnapshotCount; ++i) {
        DarkLinkDrawMaterialSnapshot& snapshot =
            g_darkLinkDrawMaterialSnapshots[i];
        if (snapshot.material == material) {
            apply_dark_link_material_output(snapshot);
            break;
        }
    }
    return HOOK_CONTINUE;
}

bool tint_goron_material_for_draw(daAlink_c* alink, J3DMaterial* material) {
    if (!g_goronTunicEquipped || alink == nullptr ||
        alink->getClothesChangeWaitTimer() != 0 || material == nullptr)
    {
        return false;
    }

    auto* bodyModel = alink->field_0x064C;
    auto* hatModel = alink->mpLinkHatModel != nullptr
                         ? alink->mpLinkHatModel->getModelData()
                         : nullptr;
    const u16* bodyIndices = nullptr;
    size_t bodyIndexCount = 0;
    const u16* hatIndices = nullptr;
    size_t hatIndexCount = 0;
    static constexpr u16 zoraBodyIndices[] = {13, 0, 1};
    static constexpr u16 magicBodyIndices[] = {11, 10, 9, 8, 6};
    static constexpr u16 casualBodyIndices[] = {7, 5};
    static constexpr u16 zoraHatIndices[] = {1};
    static constexpr u16 magicHatIndices[] = {2, 1};
    static constexpr u16 casualHatIndices[] = {0};

    if (alink->checkZoraWearAbility()) {
        bodyIndices = zoraBodyIndices;
        bodyIndexCount = std::size(zoraBodyIndices);
        hatIndices = zoraHatIndices;
        hatIndexCount = std::size(zoraHatIndices);
    } else if (alink->checkMagicArmorWearAbility()) {
        bodyIndices = magicBodyIndices;
        bodyIndexCount = std::size(magicBodyIndices);
        hatIndices = magicHatIndices;
        hatIndexCount = std::size(magicHatIndices);
    } else if (daAlink_c::checkCasualWearFlg()) {
        bodyIndices = casualBodyIndices;
        bodyIndexCount = std::size(casualBodyIndices);
        hatIndices = casualHatIndices;
        hatIndexCount = std::size(casualHatIndices);
    } else {
        return false;
    }

    auto matchesMaterial = [material](J3DModelData* model, const u16* indices,
                                      size_t count) {
        if (model == nullptr) {
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            const u16 index = indices[i];
            if (index < model->getMaterialNum() &&
                model->getMaterialNodePointer(index) == material)
            {
                tint_clothing_material(model, index);
                return true;
            }
        }
        return false;
    };

    return matchesMaterial(bodyModel, bodyIndices, bodyIndexCount) ||
           matchesMaterial(hatModel, hatIndices, hatIndexCount);
}

HookAction apply_dark_link_material_diff(ModContext*, void* args, void*, void*) {
    auto* material = mods::arg<J3DMaterial*>(args, 0);
    auto* alink = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    if (tint_goron_material_for_draw(alink, material)) {
        mods::arg_ref<u32>(args, 1) |= J3DDiffFlag_TevReg;
    }

    for (u16 i = 0; i < g_darkLinkDrawMaterialSnapshotCount; ++i) {
        DarkLinkDrawMaterialSnapshot& snapshot =
            g_darkLinkDrawMaterialSnapshots[i];
        if (snapshot.material == material) {
            apply_dark_link_material_output(snapshot);
            mods::arg_ref<u32>(args, 1) |=
                J3D_DIFF_TEVSTAGENUM(1) | J3DDiffFlag_TevReg;
            break;
        }
    }
    return HOOK_CONTINUE;
}

void set_dark_link_terminal_color(bool isPupilMaterial) {
    const GXColor color = {
        static_cast<u8>(isPupilMaterial ? 255 : 0), 0, 0, 255};
    GXSetNumTevStages(1);
    GXSetTevOrder(
        GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevColor(GX_TEVREG0, color);
    GXSetTevColorIn(
        GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C0);
    GXSetTevColorOp(
        GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(
        GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
    GXSetTevAlphaOp(
        GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}

HookAction apply_dark_link_terminal_output_before_shape_draw(
    ModContext*, void*, void*, void*) {
    J3DShapePacket* packet = j3dSys.getShapePacket();
    J3DModel* model = packet != nullptr ? packet->getModel() : nullptr;
    if (!g_darkLinkFormActive || g_darkLinkFormBlend < 1.0f)
    {
        return HOOK_CONTINUE;
    }

    J3DShape* shape = packet != nullptr ? packet->getShape() : nullptr;
    J3DMaterial* material = shape != nullptr ? shape->getMaterial() : nullptr;
    if (material == nullptr) {
        return HOOK_CONTINUE;
    }

    J3DModelData* modelData = model != nullptr ? model->getModelData() : nullptr;
    if (modelData == nullptr) {
        return HOOK_CONTINUE;
    }
    if (!is_dark_link_appearance_model_data(modelData)) {
        return HOOK_CONTINUE;
    }

    for (u16 i = 0; i < modelData->getMaterialNum(); ++i) {
        if (modelData->getMaterialNodePointer(i) != material) {
            continue;
        }

        auto* materialNames = modelData->getMaterialName();
        const char* name =
            materialNames != nullptr ? materialNames->getName(i) : nullptr;
        const bool isPupilMaterial =
            contains_case_insensitive(name, "pupil") ||
            contains_case_insensitive(name, "iris") ||
            contains_case_insensitive(name, "hitomi") ||
            material_uses_pupil_texture(modelData, material);
        set_dark_link_terminal_color(isPupilMaterial);
        break;
    }
    return HOOK_CONTINUE;
}

void double_dark_link_incoming_damage(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!g_darkLinkFormActive || alink == nullptr || alink->checkWolf()) {
        return;
    }

    *static_cast<f32*>(retval) *= 4.0f;
}

HookAction double_dark_link_sword_damage(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!g_darkLinkFormActive || alink == nullptr || alink->checkWolf()) {
        return HOOK_CONTINUE;
    }

    mods::arg_ref<int>(args, 4) *= 4;
    return HOOK_CONTINUE;
}

HookAction double_dark_link_cylinder_sword_damage(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    constexpr u32 kSwordAttackTypes = AT_TYPE_NORMAL_SWORD | AT_TYPE_MASTER_SWORD;
    if (!g_darkLinkFormActive || alink == nullptr || alink->checkWolf() ||
        (mods::arg<u32>(args, 1) & kSwordAttackTypes) == 0)
    {
        return HOOK_CONTINUE;
    }

    mods::arg_ref<int>(args, 5) *= 4;
    return HOOK_CONTINUE;
}

void restore_dark_link_model_after_draw(ModContext*, void*, void*, void*) {
    for (u16 i = 0; i < g_darkLinkDrawMaterialSnapshotCount; ++i) {
        DarkLinkDrawMaterialSnapshot& snapshot =
            g_darkLinkDrawMaterialSnapshots[i];
        if (snapshot.material == nullptr) {
            continue;
        }

        J3DTevBlock* tevBlock = snapshot.material->getTevBlock();
        if (tevBlock != nullptr) {
            tevBlock->setTevStage(snapshot.stageIndex, &snapshot.stage);
            if (snapshot.hasTevKColor) {
                tevBlock->setTevKColorSel(snapshot.stageIndex, snapshot.tevKColorSel);
            }
        }
        if (snapshot.hasTevColor) {
            snapshot.material->setTevColor(0, &snapshot.tevColor);
        }
        if (snapshot.hasTevKColor) {
            snapshot.material->setTevKColor(0, &snapshot.tevKColor);
        }
        snapshot.material = nullptr;
    }
    g_darkLinkDrawMaterialSnapshotCount = 0;
    g_darkLinkDrawModelData = nullptr;
}

void update_dark_link_form_blend() {
    const f32 step = 1.0f / static_cast<f32>(kDarkLinkTransitionFrames);
    if (g_darkLinkFormActive) {
        g_darkLinkFormBlend = std::min(1.0f, g_darkLinkFormBlend + step);
    } else {
        g_darkLinkFormBlend = std::max(0.0f, g_darkLinkFormBlend - step);
    }
}
