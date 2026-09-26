#include "pointers.hpp"

#include "RobloxModLoader/luau/generated/layout_access.hpp"

#include "lua.h"
#include "lualib.h"
#include "lobject.h"
#include "lstate.h"

#include <cstdarg>
#include <utility>
#include <cstdio>

#undef luaO_nilobject

#define RP (g_pointers->m_roblox_pointers)


int lua_gettop(lua_State* L) { return RP.lua_gettop(L); }
void lua_settop(lua_State* L, int idx) { RP.lua_settop(L, idx); }
void lua_pushvalue(lua_State* L, int idx) { RP.lua_pushvalue(L, idx); }
void lua_insert(lua_State* L, int idx) { RP.lua_insert(L, idx); }

int lua_type(lua_State* L, int idx) { return RP.lua_type(L, idx); }
int lua_isnumber(lua_State* L, int idx) { return RP.lua_isnumber(L, idx); }
int lua_isstring(lua_State* L, int idx) { return RP.lua_isstring(L, idx); }
int lua_iscfunction(lua_State* L, int idx) { return RP.lua_iscfunction(L, idx); }
int lua_isuserdata(lua_State* L, int idx) { return RP.lua_isuserdata(L, idx); }

int lua_toboolean(lua_State* L, int idx) { return RP.lua_toboolean(L, idx); }
double lua_tonumberx(lua_State* L, int idx, int* isnum) { return RP.lua_tonumberx(L, idx, isnum); }
int lua_tointegerx(lua_State* L, int idx, int* isnum) { return RP.lua_tointegerx(L, idx, isnum); }
const char* lua_tolstring(lua_State* L, int idx, size_t* len) { return RP.lua_tolstring(L, idx, len); }
const void* lua_topointer(lua_State* L, int idx) { return RP.lua_topointer(L, idx); }
int lua_objlen(lua_State* L, int idx) { return RP.lua_objlen(L, idx); }

void lua_pushnil(lua_State* L) { RP.lua_pushnil(L); }
void lua_pushnumber(lua_State* L, double n) { RP.lua_pushnumber(L, n); }
void lua_pushinteger(lua_State* L, int n) { RP.lua_pushinteger(L, n); }
void lua_pushboolean(lua_State* L, int b) { RP.lua_pushboolean(L, b); }
void lua_pushlstring(lua_State* L, const char* s, size_t len) { RP.lua_pushlstring(L, s, len); }
void lua_pushstring(lua_State* L, const char* s) { RP.lua_pushstring(L, s); }
void lua_pushcclosurek(lua_State* L, lua_CFunction fn, const char* debugname, int nup, lua_Continuation cont)
{
	RP.lua_pushcclosurek(L, fn, debugname, nup, cont);
}

void lua_createtable(lua_State* L, int narr, int nrec) { RP.lua_createtable(L, narr, nrec); }
int lua_getfield(lua_State* L, int idx, const char* k) { return RP.lua_getfield(L, idx, k); }
void lua_setfield(lua_State* L, int idx, const char* k) { RP.lua_setfield(L, idx, k); }
int lua_gettable(lua_State* L, int idx) { return RP.lua_gettable(L, idx); }
void lua_settable(lua_State* L, int idx) { RP.lua_settable(L, idx); }
int lua_rawget(lua_State* L, int idx) { return RP.lua_rawget(L, idx); }
int lua_rawgeti(lua_State* L, int idx, int n) { return RP.lua_rawgeti(L, idx, n); }
void lua_rawset(lua_State* L, int idx) { RP.lua_rawset(L, idx); }
void lua_rawseti(lua_State* L, int idx, int n) { RP.lua_rawseti(L, idx, n); }
int lua_next(lua_State* L, int idx) { return RP.lua_next(L, idx); }

int lua_getmetatable(lua_State* L, int objindex) { return RP.lua_getmetatable(L, objindex); }
int lua_setmetatable(lua_State* L, int objindex) { return RP.lua_setmetatable(L, objindex); }
int lua_getreadonly(lua_State* L, int idx) { return RP.lua_getreadonly(L, idx); }
void lua_setreadonly(lua_State* L, int idx, int enabled) { RP.lua_setreadonly(L, idx, enabled); }
const char* lua_getupvalue(lua_State* L, int funcindex, int n) { return RP.lua_getupvalue(L, funcindex, n); }
const char* lua_setupvalue(lua_State* L, int funcindex, int n) { return RP.lua_setupvalue(L, funcindex, n); }
int lua_ref(lua_State* L, int idx) { return RP.lua_ref(L, idx); }
void lua_unref(lua_State* L, int ref) { RP.lua_unref(L, ref); }

int lua_pcall(lua_State* L, int nargs, int nresults, int errfunc) { return RP.lua_pcall(L, nargs, nresults, errfunc); }
void lua_call(lua_State* L, int nargs, int nresults) { RP.lua_call(L, nargs, nresults); }
int lua_resume(lua_State* L, lua_State* from, int narg) { return RP.lua_resume(L, from, narg); }
int lua_yield(lua_State* L, int nresults) { return RP.lua_yield(L, nresults); }
lua_State* lua_newthread(lua_State* L) { return RP.lua_newthread(L); }
int lua_getinfo(lua_State* L, int level, const char* what, lua_Debug* ar) { return RP.lua_getinfo(L, level, what, ar); }

l_noret lua_error(lua_State* L)
{
	RP.lua_error(L);
	std::unreachable();
}

void luaL_register(lua_State* L, const char* libname, const luaL_Reg* l) { RP.luaL_register(L, libname, l); }
void luaL_where(lua_State* L, int lvl) { RP.luaL_where(L, lvl); }
int luaL_checkinteger(lua_State* L, int narg) { return RP.luaL_checkinteger(L, narg); }
double luaL_checknumber(lua_State* L, int narg) { return RP.luaL_checknumber(L, narg); }
const char* luaL_checklstring(lua_State* L, int narg, size_t* len) { return RP.luaL_checklstring(L, narg, len); }
void luaL_checktype(lua_State* L, int narg, int t) { RP.luaL_checktype(L, narg, t); }
void luaL_checkany(lua_State* L, int narg) { RP.luaL_checkany(L, narg); }
int luaL_optinteger(lua_State* L, int narg, int def) { return RP.luaL_optinteger(L, narg, def); }
int luaL_optboolean(lua_State* L, int narg, int def) { return RP.luaL_optboolean(L, narg, def); }
void luaL_sandboxthread(lua_State* L) { RP.luaL_sandboxthread(L); }

l_noret luaL_typeerrorL(lua_State* L, int narg, const char* tname)
{
	RP.luaL_typeerrorL(L, narg, tname);
	std::unreachable();
}

l_noret luaL_argerrorL(lua_State* L, int narg, const char* extramsg)
{
	RP.luaL_argerrorL(L, narg, extramsg);
	std::unreachable();
}

l_noret luaL_errorL(lua_State* L, const char* fmt, ...)
{
	char buffer[1024];
	va_list argp;
	va_start(argp, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, argp);
	va_end(argp);
	RP.lua_pushstring(L, buffer);
	RP.lua_error(L);
	std::unreachable();
}

Closure* luaF_newLclosure(lua_State* L, int nelems, LuaTable* e, Proto* p) { return static_cast<Closure*>(RP.luaF_newLclosure(L, nelems, e, p)); }
void luaC_barrierf(lua_State* L, GCObject* o, GCObject* v) { RP.luaC_barrierf(L, o, v); }
void luaC_barrierback(lua_State* L, GCObject* o, GCObject** gclist) { RP.luaC_barrierback(L, o, reinterpret_cast<void**>(gclist)); }
void luaC_barriertable(lua_State* L, LuaTable* t, GCObject* v) { RP.luaC_barriertable(L, t, v); }
TValue* luaH_setnum(lua_State* L, LuaTable* t, int key) { return static_cast<TValue*>(RP.luaH_setnum(L, t, key)); }

static_assert(sizeof(TValue) == rml::luau::access::tvalue_size,
              "studio walks the stack in steps this build does not agree with");

const TValue* luaA_toobject(lua_State* L, int idx)
{
	const auto* state = rml::luau::access::state(L);
	auto* base = reinterpret_cast<TValue*>(state->base);
	auto* top = reinterpret_cast<TValue*>(state->top);

	if (idx > 0)
	{
		TValue* o = base + (idx - 1);
		return o < top ? o : static_cast<const TValue*>(RP.luaO_nilobject);
	}
	if (idx > LUA_REGISTRYINDEX)
		return top + idx;
	return static_cast<const TValue*>(RP.luaA_pseudo2addr(L, idx));
}

#undef RP
