#include "natives.h"
#include "errors.h"
#include "modules/containers.h"
#include "modules/strings.h"
#include "objects/dyn_object.h"

#include <cstring>
#include <algorithm>
#include <limits>

typedef strings::cell_string cell_string;

namespace Natives
{
	// native String:str_new(const str[], str_create_mode:mode=str_preserve);
	AMX_DEFINE_NATIVE(str_new, 1)
	{
		cell *addr;
		amx_GetAddr(amx, params[1], &addr);
		int flags = optparam(2, 0);
		return strings::create(addr, flags & 1, flags & 2);
	}

	// native String:str_new_arr(const arr[], size=sizeof(arr), str_create_mode:mode=str_preserve);
	AMX_DEFINE_NATIVE(str_new_arr, 2)
	{
		cell *addr;
		amx_GetAddr(amx, params[1], &addr);
		int flags = optparam(3, 0);
		return strings::create(addr, static_cast<size_t>(params[2]), false, flags & 1, flags & 2);
	}

	// native String:str_new_static(const str[], str_create_mode:mode=str_preserve, size=sizeof(str));
	AMX_DEFINE_NATIVE(str_new_static, 3)
	{
		cell *addr;
		amx_GetAddr(amx, params[1], &addr);
		int flags = params[2];
		cell size = (addr[0] & 0xFF000000) ? params[3] : params[3] - 1;
		if(size < 0) size = 0;
		return strings::create(addr, static_cast<size_t>(size), true, flags & 1, flags & 2);
	}

	// native String:str_new_buf(size);
	AMX_DEFINE_NATIVE(str_new_buf, 1)
	{
		cell size = params[1];
		return strings::pool.get_id(strings::pool.add(cell_string(size - 1, 0)));
	}

	// native AmxString:str_addr(StringTag:str);
	AMX_DEFINE_NATIVE(str_addr, 1)
	{
		decltype(strings::pool)::ref_container *str;
		if(!strings::pool.get_by_id(params[1], str) && str != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		return strings::pool.get_address(amx, *str);
	}

	// native AmxStringBuffer:str_buf_addr(StringTag:str);
	AMX_DEFINE_NATIVE(str_buf_addr, 1)
	{
		decltype(strings::pool)::ref_container *str;
		if(!strings::pool.get_by_id(params[1], str) && str != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		return strings::pool.get_inner_address(amx, *str);
	}

	// native String:str_acquire(StringTag:str);
	AMX_DEFINE_NATIVE(str_acquire, 1)
	{
		decltype(strings::pool)::ref_container *str;
		if(!strings::pool.get_by_id(params[1], str)) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		if(!strings::pool.acquire_ref(*str)) amx_LogicError(errors::cannot_acquire, "string", params[1]);
		return params[1];
	}

	// native String:str_release(StringTag:str);
	AMX_DEFINE_NATIVE(str_release, 1)
	{
		decltype(strings::pool)::ref_container *str;
		if(!strings::pool.get_by_id(params[1], str)) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		if(!strings::pool.release_ref(*str)) amx_LogicError(errors::cannot_release, "string", params[1]);
		return params[1];
	}

	// native str_delete(StringTag:str);
	AMX_DEFINE_NATIVE(str_delete, 1)
	{
		if(!strings::pool.remove_by_id(params[1])) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		return 1;
	}

	// native bool:str_valid(StringTag:str);
	AMX_DEFINE_NATIVE(str_valid, 1)
	{
		cell_string *str;
		return strings::pool.get_by_id(params[1], str);
	}

	// native String:str_clone(StringTag:str);
	AMX_DEFINE_NATIVE(str_clone, 1)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str)) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		return strings::pool.get_id(strings::pool.add(cell_string(*str)));
	}


	// native String:str_cat(StringTag:str1, StringTag:str2);
	AMX_DEFINE_NATIVE(str_cat, 2)
	{
		cell_string *str1, *str2;
		if((!strings::pool.get_by_id(params[1], str1) && str1 != nullptr)) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		if((!strings::pool.get_by_id(params[2], str2) && str2 != nullptr)) amx_LogicError(errors::pointer_invalid, "string", params[2]);
		
		if(str1 == nullptr && str2 == nullptr)
		{
			return strings::pool.get_id(strings::pool.add());
		}
		if(str1 == nullptr)
		{
			return strings::pool.get_id(strings::pool.add(cell_string(*str2)));
		}
		if(str2 == nullptr)
		{
			return strings::pool.get_id(strings::pool.add(cell_string(*str1)));
		}

		auto str = *str1 + *str2;
		return strings::pool.get_id(strings::pool.add(std::move(str)));
	}

	// native String:str_val(AnyTag:val, tag=tagof(value));
	AMX_DEFINE_NATIVE(str_val, 2)
	{
		dyn_object obj{amx, params[1], params[2]};
		return strings::pool.get_id(strings::pool.add(obj.to_string()));
	}

	// native String:str_val_arr(AnyTag:value[], size=sizeof(value), tag_id=tagof(value));
	AMX_DEFINE_NATIVE(str_val_arr, 3)
	{
		cell *addr;
		amx_GetAddr(amx, params[1], &addr);
		dyn_object obj{amx, addr, params[2], params[3]};
		return strings::pool.get_id(strings::pool.add(obj.to_string()));
	}

	static cell AMX_NATIVE_CALL str_split_base(AMX *amx, cell_string *str, const cell_string &delims)
	{
		auto list = list_pool.add();

		cell_string::size_type last_pos = 0;
		while(last_pos != cell_string::npos)
		{
			auto it = std::find_first_of(str->begin() + last_pos, str->end(), delims.begin(), delims.end());

			size_t pos;
			if(it == str->end())
			{
				pos = cell_string::npos;
			}else{
				pos = std::distance(str->begin(), it);
			}

			auto sub = &(*str)[last_pos];
			cell_string::size_type size;
			if(pos != cell_string::npos)
			{
				size = pos - last_pos;
			}else{
				size = str->size() - last_pos;
			}
			cell old = sub[size];
			sub[size] = 0;
			list->push_back(dyn_object(sub, size + 1, tags::find_tag(tags::tag_char)));
			sub[size] = old;

			if(pos != cell_string::npos)
			{
				last_pos = pos + 1;
			}else{
				last_pos = cell_string::npos;
			}
		}

		return list_pool.get_id(list);
	}

	// native List:str_split(StringTag:str, const delims[]);
	AMX_DEFINE_NATIVE(str_split, 2)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str) && str != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		if(str == nullptr)
		{
			return list_pool.get_id(list_pool.add());
		}
		cell *addr;
		amx_GetAddr(amx, params[2], &addr);
		cell_string delims(addr);
		return str_split_base(amx, str, delims);
	}

	// native List:str_split_s(StringTag:str, StringTag:delims);
	AMX_DEFINE_NATIVE(str_split_s, 2)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str) && str != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		cell_string *delims;
		if(!strings::pool.get_by_id(params[2], delims) && delims != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[2]);
		if(str == nullptr)
		{
			return list_pool.get_id(list_pool.add());
		}
		if(delims == nullptr)
		{
			return str_split_base(amx, str, cell_string());
		}else{
			return str_split_base(amx, str, *delims);
		}
	}

	static cell AMX_NATIVE_CALL str_join_base(AMX *amx, list_t *list, const cell_string &delim)
	{
		auto &str = strings::pool.add();
		bool first = true;
		for(auto &obj : *list)
		{
			if(first)
			{
				first = false;
			} else {
				str->append(delim);
			}
			str->append(obj.to_string());
		}

		return strings::pool.get_id(str);
	}

	// native String:str_join(List:list, const delim[]);
	AMX_DEFINE_NATIVE(str_join, 2)
	{
		list_t *list;
		if(!list_pool.get_by_id(params[1], list)) amx_LogicError(errors::pointer_invalid, "list", params[1]);
		cell *addr;
		amx_GetAddr(amx, params[2], &addr);
		cell_string delim(addr);
		return str_join_base(amx, list, delim);
	}

	// native String:str_join_s(List:list, StringTag:delim);
	AMX_DEFINE_NATIVE(str_join_s, 2)
	{
		list_t *list;
		if(!list_pool.get_by_id(params[1], list)) amx_LogicError(errors::pointer_invalid, "list", params[1]);
		cell_string *delim;
		if(!strings::pool.get_by_id(params[1], delim) && delim != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[2]);
		if(delim == nullptr)
		{
			return str_join_base(amx, list, cell_string());
		}else{
			return str_join_base(amx, list, *delim);
		}
	}

	// native str_len(StringTag:str);
	AMX_DEFINE_NATIVE(str_len, 1)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str) && str != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		if(str == nullptr) return 0;
		return static_cast<cell>(str->size());
	}

	// native str_get(StringTag:str, buffer[], size=sizeof(buffer), start=0, end=cellmax);
	AMX_DEFINE_NATIVE(str_get, 3)
	{
		if(params[3] == 0) return 0;

		cell *addr;
		amx_GetAddr(amx, params[2], &addr);

		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str) && str != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);

		if(str == nullptr)
		{
			addr[0] = 0;
			return 0;
		}

		cell start = optparam(4, 0);
		cell end = optparam(5, std::numeric_limits<cell>::max());

		if(!strings::clamp_range(*str, start, end))
		{
			return 0;
		}

		cell len = end - start;
		if(len >= params[3])
		{
			len = params[3] - 1;
		}

		if(len >= 0)
		{
			std::memcpy(addr, &(*str)[0], len * sizeof(cell));
			addr[len] = 0;
			return len;
		}
		return 0;
	}

	// native str_getc(StringTag:str, pos);
	AMX_DEFINE_NATIVE(str_getc, 2)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str)) return 0xFFFFFF00;

		if(strings::clamp_pos(*str, params[2]))
		{
			return (*str)[params[2]];
		}
		return 0xFFFFFF00;
	}

	// native str_setc(StringTag:str, pos, value);
	AMX_DEFINE_NATIVE(str_setc, 3)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str)) return 0xFFFFFF00;

		if(strings::clamp_pos(*str, params[2]))
		{
			cell c = (*str)[params[2]];
			(*str)[params[2]] = params[3];
			return c;
		}
		return 0xFFFFFF00;
	}

	// native String:str_set(StringTag:target, StringTag:other);
	AMX_DEFINE_NATIVE(str_set, 2)
	{
		cell_string *str1;
		if(!strings::pool.get_by_id(params[1], str1)) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		cell_string *str2;
		if(!strings::pool.get_by_id(params[2], str2) && str2 != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[2]);

		if(str2 == nullptr)
		{
			str1->clear();
		}else{
			str1->assign(*str2);
		}
		return params[1];
	}

	// native String:str_append(StringTag:target, StringTag:other);
	AMX_DEFINE_NATIVE(str_append, 2)
	{
		cell_string *str1;
		if(!strings::pool.get_by_id(params[1], str1)) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		cell_string *str2;
		if(!strings::pool.get_by_id(params[2], str2) && str2 != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[2]);
		if(str2 != nullptr)
		{
			str1->append(*str2);
		}
		return params[1];
	}

	// native String:str_ins(StringTag:target, StringTag:other, pos);
	AMX_DEFINE_NATIVE(str_ins, 3)
	{
		cell_string *str1;
		if(!strings::pool.get_by_id(params[1], str1)) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		cell_string *str2;
		if(!strings::pool.get_by_id(params[2], str2) && str2 != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[2]);
		if(str2 != nullptr)
		{
			strings::clamp_pos(*str1, params[3]);
			str1->insert(params[3], *str2);
		}
		return params[1];
	}

	// native String:str_del(StringTag:target, start=0, end=cellmax);
	AMX_DEFINE_NATIVE(str_del, 1)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str)) amx_LogicError(errors::pointer_invalid, "string", params[1]);

		cell start = optparam(2, 0);
		cell end = optparam(3, std::numeric_limits<cell>::max());

		if(strings::clamp_range(*str, start, end))
		{
			str->erase(start, end - start);
		}else{
			str->erase(start, -1);
			str->erase(0, end);
		}
		return params[1];
	}

	// native String:str_sub(StringTag:str, start=0, end=cellmax);
	AMX_DEFINE_NATIVE(str_sub, 1)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str) && str != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		if(str == nullptr) return strings::pool.get_id(strings::pool.add());

		cell start = optparam(2, 0);
		cell end = optparam(3, std::numeric_limits<cell>::max());

		if(strings::clamp_range(*str, start, end))
		{
			auto substr = str->substr(start, end - start);
			return strings::pool.get_id(strings::pool.add(std::move(substr)));
		}
		return 0;
	}

	// native bool:str_cmp(StringTag:str1, StringTag:str2);
	AMX_DEFINE_NATIVE(str_cmp, 2)
	{
		cell_string *str1;
		if(!strings::pool.get_by_id(params[1], str1) && str1 != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		cell_string *str2;
		if(!strings::pool.get_by_id(params[2], str2) && str2 != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[2]);

		if(str1 == nullptr && str2 == nullptr) return 1;
		if(str1 == nullptr)
		{
			return str2->size() == 0;
		}
		if(str2 == nullptr)
		{
			return str1->size() == 0;
		}
		return str1->compare(*str2);
	}

	// native bool:str_empty(StringTag:str);
	AMX_DEFINE_NATIVE(str_empty, 1)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str) && str != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		if(str == nullptr) return 1;
		return str->empty();
	}

	// native bool:str_eq(StringTag:str1, StringTag:str2);
	AMX_DEFINE_NATIVE(str_eq, 2)
	{
		cell_string *str1;
		if(!strings::pool.get_by_id(params[1], str1) && str1 != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		cell_string *str2;
		if(!strings::pool.get_by_id(params[2], str2) && str2 != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[2]);

		if(str1 == nullptr && str2 == nullptr) return 1;
		if(str1 == nullptr)
		{
			return str2->size() == 0;
		}
		if(str2 == nullptr)
		{
			return str1->size() == 0;
		}
		return *str1 == *str2;
	}

	// native str_findc(StringTag:str, value, offset=0);
	AMX_DEFINE_NATIVE(str_findc, 2)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str) && str != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		if(str == nullptr) return -1;

		cell offset = optparam(3, 0);
		strings::clamp_pos(*str, offset);
		return str->find(params[2], static_cast<size_t>(offset));
	}

	// native str_find(StringTag:str, StringTag:value, offset=0);
	AMX_DEFINE_NATIVE(str_find, 2)
	{
		cell_string *str1;
		if(!strings::pool.get_by_id(params[1], str1) && str1 != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		cell_string *str2;
		if(!strings::pool.get_by_id(params[2], str2)) amx_LogicError(errors::pointer_invalid, "string", params[2]);
		if(str1 == nullptr) return str2->empty() ? 0 : -1;

		cell offset = optparam(3, 0);
		strings::clamp_pos(*str1, offset);

		return static_cast<cell>(str1->find(*str2, static_cast<size_t>(offset)));
	}

	// native String:str_clear(StringTag:str);
	AMX_DEFINE_NATIVE(str_clear, 1)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str) && str != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		str->clear();
		return params[1];
	}

	// native String:str_resize(StringTag:str, size, padding=0);
	AMX_DEFINE_NATIVE(str_resize, 2)
	{
		if(params[2] < 0) amx_LogicError(errors::out_of_range, "size");
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str)) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		str->resize(static_cast<size_t>(params[2]), optparam(3, 0));
		return params[1];
	}

	// native String:str_format(const format[], {StringTags,Float,_}:...);
	AMX_DEFINE_NATIVE(str_format, 1)
	{
		cell *format;
		amx_GetAddr(amx, params[1], &format);
		int flen;
		amx_StrLen(format, &flen);

		auto &str = strings::pool.add();
		strings::format(amx, *str, format, flen, params[0] - 1, params + 2);
		return strings::pool.get_id(str);
	}

	// native String:str_format_s(StringTag:format, {StringTags,Float,_}:...);
	AMX_DEFINE_NATIVE(str_format_s, 1)
	{
		cell_string *strformat;
		if(!strings::pool.get_by_id(params[1], strformat) && strformat != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		auto &str = strings::pool.add();
		if(strformat != nullptr)
		{
			strings::format(amx, *str, &(*strformat)[0], strformat->size(), params[0] / sizeof(cell) - 1, params + 2);
		}
		return strings::pool.get_id(str);
	}

	// native String:str_set_format(StringTag:target, const format[], {StringTags,Float,_}:...);
	AMX_DEFINE_NATIVE(str_set_format, 2)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str)) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		str->clear();

		cell *format;
		amx_GetAddr(amx, params[2], &format);
		int flen;
		amx_StrLen(format, &flen);


		strings::format(amx, *str, format, flen, params[0] / sizeof(cell) - 2, params + 3);
		return params[1];
	}

	// native String:str_set_format_s(StringTag:target, StringTag:format, {StringTags,Float,_}:...);
	AMX_DEFINE_NATIVE(str_set_format_s, 2)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str)) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		str->clear();
		
		cell_string *strformat;
		if(!strings::pool.get_by_id(params[2], strformat) && strformat != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[2]);
		if(strformat != nullptr)
		{
			strings::format(amx, *str, &(*strformat)[0], strformat->size(), params[0] / sizeof(cell) - 2, params + 3);
		}
		return params[1];
	}

	static cell to_lower(cell c)
	{
		if(65 <= c && c <= 90) return c + 32;
		return c;
	}

	static cell to_upper(cell c)
	{
		if(97 <= c && c <= 122) return c - 32;
		return c;
	}

	// native String:str_to_lower(StringTag:str);
	AMX_DEFINE_NATIVE(str_to_lower, 1)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str) && str != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		if(str == nullptr)
		{
			return strings::pool.get_id(strings::pool.add());
		}
		auto str2 = *str;
		std::transform(str2.begin(), str2.end(), str2.begin(), to_lower);
		return strings::pool.get_id(strings::pool.add(std::move(str2)));
	}

	// native String:str_to_upper(StringTag:str);
	AMX_DEFINE_NATIVE(str_to_upper, 1)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str) && str != nullptr) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		if(str == nullptr)
		{
			return strings::pool.get_id(strings::pool.add());
		}
		auto str2 = *str;
		std::transform(str2.begin(), str2.end(), str2.begin(), to_upper);
		return strings::pool.get_id(strings::pool.add(std::move(str2)));
	}

	// native String:str_set_to_lower(StringTag:str);
	AMX_DEFINE_NATIVE(str_set_to_lower, 1)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str)) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		std::transform(str->begin(), str->end(), str->begin(), to_lower);
		return params[1];
	}

	// native String:str_set_to_upper(StringTag:str);
	AMX_DEFINE_NATIVE(str_set_to_upper, 1)
	{
		cell_string *str;
		if(!strings::pool.get_by_id(params[1], str)) amx_LogicError(errors::pointer_invalid, "string", params[1]);
		std::transform(str->begin(), str->end(), str->begin(), to_upper);
		return params[1];
	}
}

static AMX_NATIVE_INFO native_list[] =
{
	AMX_DECLARE_NATIVE(str_new),
	AMX_DECLARE_NATIVE(str_new_arr),
	AMX_DECLARE_NATIVE(str_new_static),
	AMX_DECLARE_NATIVE(str_new_buf),
	AMX_DECLARE_NATIVE(str_addr),
	AMX_DECLARE_NATIVE(str_buf_addr),
	AMX_DECLARE_NATIVE(str_acquire),
	AMX_DECLARE_NATIVE(str_release),
	AMX_DECLARE_NATIVE(str_delete),
	AMX_DECLARE_NATIVE(str_valid),
	AMX_DECLARE_NATIVE(str_clone),

	AMX_DECLARE_NATIVE(str_len),
	AMX_DECLARE_NATIVE(str_get),
	AMX_DECLARE_NATIVE(str_getc),
	AMX_DECLARE_NATIVE(str_setc),
	AMX_DECLARE_NATIVE(str_cmp),
	AMX_DECLARE_NATIVE(str_empty),
	AMX_DECLARE_NATIVE(str_eq),
	AMX_DECLARE_NATIVE(str_findc),
	AMX_DECLARE_NATIVE(str_find),

	AMX_DECLARE_NATIVE(str_cat),
	AMX_DECLARE_NATIVE(str_sub),
	AMX_DECLARE_NATIVE(str_val),
	AMX_DECLARE_NATIVE(str_val_arr),
	AMX_DECLARE_NATIVE(str_split),
	AMX_DECLARE_NATIVE(str_split_s),
	AMX_DECLARE_NATIVE(str_join),
	AMX_DECLARE_NATIVE(str_join_s),
	AMX_DECLARE_NATIVE(str_to_lower),
	AMX_DECLARE_NATIVE(str_to_upper),

	AMX_DECLARE_NATIVE(str_set),
	AMX_DECLARE_NATIVE(str_append),
	AMX_DECLARE_NATIVE(str_ins),
	AMX_DECLARE_NATIVE(str_del),
	AMX_DECLARE_NATIVE(str_clear),
	AMX_DECLARE_NATIVE(str_resize),
	AMX_DECLARE_NATIVE(str_set_to_lower),
	AMX_DECLARE_NATIVE(str_set_to_upper),

	AMX_DECLARE_NATIVE(str_format),
	AMX_DECLARE_NATIVE(str_format_s),
	AMX_DECLARE_NATIVE(str_set_format),
	AMX_DECLARE_NATIVE(str_set_format_s),
};

int RegisterStringsNatives(AMX *amx)
{
	return amx_Register(amx, native_list, sizeof(native_list) / sizeof(*native_list));
}
