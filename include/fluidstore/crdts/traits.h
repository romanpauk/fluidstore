#pragma once

namespace crdt
{
	template < typename T > struct is_crdt_type: std::false_type {};
}