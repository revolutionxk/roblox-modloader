#pragma once
#include "RobloxModLoader/roblox/data_model_type.hpp"
#include "RobloxModLoader/internal/common.hpp"

#include <shared_mutex>
#include <unordered_map>

namespace RBX {
    class DataModel;
    class ScriptContext;
}

namespace rml {
    class DataModelRegistry final {
    public:
        DataModelRegistry() = default;

        ~DataModelRegistry() = default;

        DataModelRegistry(const DataModelRegistry &) = delete;

        DataModelRegistry &operator=(const DataModelRegistry &) = delete;

        DataModelRegistry(DataModelRegistry &&) noexcept = delete;

        DataModelRegistry &operator=(DataModelRegistry &&) noexcept = delete;

        void set_data_model(RBX::DataModelType type, RBX::DataModel *data_model, RBX::ScriptContext *script_context);

        const RBX::DataModel *get_data_model_by_type(RBX::DataModelType type) noexcept;

        void cleanup_data_model(RBX::DataModelType data_model_type);

    private:
        std::shared_mutex m_data_model_mutex;
        std::unordered_map<RBX::DataModelType, const RBX::DataModel *> m_data_models;
    };
}
