#include "RobloxModLoader/platform/graphics/output_readback.hpp"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <objc/runtime.h>

#include <atomic>
#include <map>
#include <mutex>
#include <utility>

namespace rml::platform
{
	static std::atomic<bool> g_readable{false};
	static std::atomic<IMP> g_next_drawable{nullptr};
	static std::once_flag g_install;
	static std::mutex g_mutex;
	static std::map<std::pair<std::uint32_t, std::uint32_t>, bool> g_drawables;
	static char g_changed_key;

	static id swizzled_next_drawable(CAMetalLayer* self, SEL command)
	{
		try
		{
			const bool readable = g_readable.load();
			const bool changed = objc_getAssociatedObject(self, &g_changed_key) != nil;
			if (readable && self.framebufferOnly)
			{
				self.framebufferOnly = NO;
				objc_setAssociatedObject(self, &g_changed_key, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
			}
			else if (!readable && changed)
			{
				self.framebufferOnly = YES;
				objc_setAssociatedObject(self, &g_changed_key, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
			}
		}
		catch (...)
		{
		}

		const IMP original = g_next_drawable.load();
		id<CAMetalDrawable> drawable = reinterpret_cast<id (*)(id, SEL)>(original)(self, command);
		try
		{
			if (drawable)
			{
				id<MTLTexture> texture = drawable.texture;
				std::scoped_lock lock(g_mutex);
				g_drawables[{static_cast<std::uint32_t>(texture.width), static_cast<std::uint32_t>(texture.height)}] = !texture.framebufferOnly;
			}
		}
		catch (...)
		{
		}
		return drawable;
	}

	bool set_output_readable(const bool readable)
	{
		std::call_once(g_install, [] {
			Method method = class_getInstanceMethod([CAMetalLayer class], @selector(nextDrawable));
			if (!method)
				return;
			g_next_drawable.store(method_getImplementation(method));
			method_setImplementation(method, reinterpret_cast<IMP>(&swizzled_next_drawable));
		});
		if (!g_next_drawable.load())
			return false;
		g_readable.store(readable);
		return true;
	}

	bool output_readable(const std::uint32_t width, const std::uint32_t height)
	{
		std::scoped_lock lock(g_mutex);
		const auto it = g_drawables.find({width, height});
		return it != g_drawables.end() && it->second;
	}
}
