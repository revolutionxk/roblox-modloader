#include "data_model_registry.hpp"

RML_LOG_SCOPE("DataModelRegistry");

namespace rml
{
	void DataModelRegistry::set_data_model(const RBX::DataModelType type, RBX::DataModel* data_model, RBX::ScriptContext* script_context)
	{
		const RBX::DataModel* old_data_model = nullptr;
		{
			std::shared_lock lock(m_data_model_mutex);
			if (const auto it = m_data_models.find(type); it != m_data_models.end())
			{
				old_data_model = it->second;
			}
		}

		if (old_data_model && old_data_model != data_model)
		{
			RML_INFO("DataModel type {} changed, cleaning up old instance", std::to_underlying(type));
		}

		{
			std::unique_lock lock(m_data_model_mutex);
			if (data_model)
			{
				m_data_models[type] = data_model;
			}
			else
			{
				m_data_models.erase(type);
			}
		}
	}

	const RBX::DataModel* DataModelRegistry::get_data_model_by_type(const RBX::DataModelType type) noexcept
	{
		std::shared_lock lock(m_data_model_mutex);

		const auto it = m_data_models.find(type);
		if (it == m_data_models.end())
			return nullptr;

		return it->second;
	}

	void DataModelRegistry::cleanup_data_model(const RBX::DataModelType data_model_type)
	{
		RML_INFO("Cleaning up DataModel type: {}", std::to_underlying(data_model_type));

		{
			std::unique_lock lock(m_data_model_mutex);
			m_data_models.erase(data_model_type);
		}

		RML_INFO("DataModel type {} cleanup completed", std::to_underlying(data_model_type));
	}
}
