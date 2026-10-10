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
	static IMP g_next_drawable;
	static std::mutex g_mutex;
	static std::map<std::pair<std::uint32_t, std::uint32_t>, bool> g_drawables;
	static char g_changed_key;

	static id swizzled_next_drawable(CAMetalLayer* self, SEL command)
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

		id<CAMetalDrawable> drawable = reinterpret_cast<id (*)(id, SEL)>(g_next_drawable)(self, command);
		if (drawable)
		{
			id<MTLTexture> texture = drawable.texture;
			std::scoped_lock lock(g_mutex);
			g_drawables[{static_cast<std::uint32_t>(texture.width), static_cast<std::uint32_t>(texture.height)}] = !texture.framebufferOnly;
		}
		return drawable;
	}

	bool set_output_readable(const bool readable)
	{
		if (!g_next_drawable)
		{
			Method method = class_getInstanceMethod([CAMetalLayer class], @selector(nextDrawable));
			if (!method)
				return false;
			g_next_drawable = method_setImplementation(method, reinterpret_cast<IMP>(&swizzled_next_drawable));
			if (!g_next_drawable)
				return false;
		}
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
