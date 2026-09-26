#pragma once

#include "RobloxModLoader/roblox/data_model_type.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"
#include "data_model_prop.hpp"
#include "data_model_serialize.hpp"
#include "service_provider.hpp"
#include "task_scheduler_arbiter.hpp"
#include "verb_container.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace RBX
{
	class DataModelJob;

	enum class DataModelType : std::int32_t
	{
		Edit = 0,
		Client = 1,
		Server = 2,
		Standalone = 3,
		Null = 4,
	};

	class IDataState
	{
	public:
		virtual void set_dirty(bool dirty) = 0;

		virtual bool is_dirty() const = 0;
	};

	namespace Diagnostics
	{
		template<typename T>
		class Countable
		{
		};
	}

	class PageMilestoneKey
	{
	public:
		std::uint64_t value;
	};

	class PageMilestoneSubscribers
	{
	public:
		void* inline_storage[3];
	};

	class PageMilestoneKeyHash
	{
	public:
		std::size_t operator()(const PageMilestoneKey& key) const
		{
			return key.value;
		}
	};

	class RML_ENGINE_CLASS DataModel : public IDataState,
	                  public Diagnostics::Countable<DataModel>,
	                  public TaskSchedulerArbiter,
	                  public Described<DataModel, ServiceProvider>,
	                  public DataModelProp
	{
	public:
		void set_dirty(bool dirty) override = 0;

		bool is_dirty() const override = 0;

		void write_all_properties_for_change_tracking(DataModelChangeTracking::ChangeTracker* tracker,
		    DataModelChangeTracking::DeltaTypeTag tag) override = 0;

		void* get_as_internal(Reflection::InterfaceId interface_id) const override = 0;

		~DataModel() override = default;

		const char* arbiter_name() const override = 0;

		void on_connected_to_change_signal_from_lua(const RBX::Lua::EventInstance& event,
		    lua_State* state) override = 0;

		bool verify_add_child(const Instance* child) const override = 0;

		bool ask_add_child(const Instance* child) const override = 0;

		void on_child_added(Instance* child) override = 0;

		void on_child_changed(Instance* child,
		    const Reflection::PropertyDescriptor& descriptor) override = 0;

		void on_descendant_added(Instance* descendant) override = 0;

		void on_descendant_removing(const std::shared_ptr<Instance>& descendant) override = 0;

		void on_pre_acquire() override = 0;

		void on_acquire() override = 0;

		void* on_borrow() override = 0;

		void on_return(std::size_t token) override = 0;

		void on_release() override = 0;

		bool can_find_service() const override = 0;

		bool can_create_service() const override = 0;

		std::unique_ptr<SerializedExternalRefs> serialized_external_refs;

	private:
		boost::intrusive_ptr<rbx::signals::slots_holder> reserved_slots_0[18];

	public:
		RSL::Mutex page_milestone_mutex;
		std::unordered_map<PageMilestoneKey, PageMilestoneSubscribers, PageMilestoneKeyHash>
		    page_milestone_registry;

	private:
		boost::intrusive_ptr<rbx::signals::slots_holder> reserved_slots_1[12];

	public:
		std::shared_ptr<IDataModelSerialize> data_model_serialize;

	private:
		std::byte reserved_before_type[0x48];

	public:
		DataModelType type;

	private:
		std::uint16_t reserved_after_type;

	public:
		VerbContainer* verb_container;

		static DataModel* from_job(const DataModelJob* job);
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(DataModel, workspace, 0x350);
	RML_ASSERT_OFFSET(DataModel, serialized_external_refs, 0x3B8);
	RML_ASSERT_OFFSET(DataModel, page_milestone_mutex, 0x450);
	RML_ASSERT_OFFSET(DataModel, page_milestone_registry, 0x458);
	RML_ASSERT_OFFSET(DataModel, data_model_serialize, 0x4F8);
	RML_ASSERT_OFFSET(DataModel, type, 0x550);
	RML_ASSERT_OFFSET(DataModel, verb_container, 0x558);
#else
	RML_ASSERT_OFFSET(DataModel, workspace, 0x340);
	RML_ASSERT_OFFSET(DataModel, serialized_external_refs, 0x3A0);
	RML_ASSERT_OFFSET(DataModel, page_milestone_mutex, 0x438);
	RML_ASSERT_OFFSET(DataModel, page_milestone_registry, 0x440);
	RML_ASSERT_OFFSET(DataModel, data_model_serialize, 0x4C8);
	RML_ASSERT_OFFSET(DataModel, type, 0x520);
	RML_ASSERT_OFFSET(DataModel, verb_container, 0x528);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()
}
