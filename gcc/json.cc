/* JSON trees
   Copyright (C) 2017-2023 Free Software Foundation, Inc.
   Contributed by David Malcolm <dmalcolm@redhat.com>.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "json.h"
#include "pretty-print.h"
#include "math.h"
#include "selftest.h"

using namespace json;

/* class json::value.  */

/* Dump this json::value tree to OUTF.

   The key/value pairs of json::objects are printed in the order
   in which the keys were originally inserted.  */

void
value::dump (FILE *outf, bool formatted) const
{
  pretty_printer pp;
  pp_buffer (&pp)->stream = outf;
  print (&pp, formatted);
  pp_flush (&pp);
}

/* class json::object, a subclass of json::value, representing
   an ordered collection of key/value pairs.  */

/* json:object's dtor.  */

object::~object ()
{
  for (map_t::iterator it = m_map.begin (); it != m_map.end (); ++it)
    {
      free (const_cast <char *>((*it).first));
      delete ((*it).second);
    }
}

/* Implementation of json::value::print for json::object.  */

void
object::print (pretty_printer *pp, bool formatted) const
{
  pp_character (pp, '{');
  if (formatted)
    pp_indentation (pp) += 1;

  /* Iterate in the order that the keys were inserted.  */
  unsigned i;
  const char *key;
  FOR_EACH_VEC_ELT (m_keys, i, key)
    {
      if (i > 0)
	{
	  pp_string (pp, ",");
	  if (formatted)
	    {
	      pp_newline (pp);
	      pp_indent (pp);
	    }
	  else
	    pp_space (pp);
	}
      map_t &mut_map = const_cast<map_t &> (m_map);
      value *value = *mut_map.get (key);
      pp_doublequote (pp);
      pp_string (pp, key); // FIXME: escaping?
      pp_doublequote (pp);
      pp_string (pp, ": ");
      const int indent = strlen (key) + 4;
      if (formatted)
	pp_indentation (pp) += indent;
      value->print (pp, formatted);
      if (formatted)
	pp_indentation (pp) -= indent;
    }
  if (formatted)
    pp_indentation (pp) -= 1;
  pp_character (pp, '}');
}

/* Set the json::value * for KEY, taking ownership of V
   (and taking a copy of KEY if necessary).  */

void
object::set (const char *key, value *v)
{
  gcc_assert (key);
  gcc_assert (v);

  value **ptr = m_map.get (key);
  if (ptr)
    {
      /* If the key is already present, delete the existing value
	 and overwrite it.  */
      delete *ptr;
      *ptr = v;
    }
  else
    {
      /* If the key wasn't already present, take a copy of the key,
	 and store the value.  */
      char *owned_key = xstrdup (key);
      m_map.put (owned_key, v);
      m_keys.safe_push (owned_key);
    }
}

/* Get the json::value * for KEY.

   The object retains ownership of the value.  */

value *
object::get (const char *key) const
{
  gcc_assert (key);

  value **ptr = const_cast <map_t &> (m_map).get (key);
  if (ptr)
    return *ptr;
  else
    return NULL;
}

/* Set value of KEY within this object to a JSON
   string value based on UTF8_VALUE.  */

void
object::set_string (const char *key, const char *utf8_value)
{
  set (key, new json::string (utf8_value));
}

/* Set value of KEY within this object to a JSON
   integer value based on V.  */

void
object::set_integer (const char *key, long v)
{
  set (key, new json::integer_number (v));
}

/* Set value of KEY within this object to a JSON
   floating point value based on V.  */

void
object::set_float (const char *key, double v)
{
  set (key, new json::float_number (v));
}

/* Set value of KEY within this object to the JSON
   literal true or false, based on V.  */

void
object::set_bool (const char *key, bool v)
{
  set (key, new json::literal (v));
}

/* class json::array, a subclass of json::value, representing
   an ordered collection of values.  */

/* json::array's dtor.  */

array::~array ()
{
  unsigned i;
  value *v;
  FOR_EACH_VEC_ELT (m_elements, i, v)
    delete v;
}

/* Implementation of json::value::print for json::array.  */

void
array::print (pretty_printer *pp, bool formatted) const
{
  pp_character (pp, '[');
  if (formatted)
    pp_indentation (pp) += 1;
  unsigned i;
  value *v;
  FOR_EACH_VEC_ELT (m_elements, i, v)
    {
      if (i)
	{
	  pp_string (pp, ",");
	  if (formatted)
	    {
	      pp_newline (pp);
	      pp_indent (pp);
	    }
	  else
	    pp_space (pp);
	}
      v->print (pp, formatted);
    }
  if (formatted)
    pp_indentation (pp) -= 1;
  pp_character (pp, ']');
}

/* Append non-NULL value V to a json::array, taking ownership of V.  */

void
array::append (value *v)
{
  gcc_assert (v);
  m_elements.safe_push (v);
}

/* class json::float_number, a subclass of json::value, wrapping a double.  */

/* Implementation of json::value::print for json::float_number.  */

void
float_number::print (pretty_printer *pp,
		     bool formatted ATTRIBUTE_UNUSED) const
{
  char tmp[1024];
  snprintf (tmp, sizeof (tmp), "%g", m_value);
  pp_string (pp, tmp);
}

/* class json::integer_number, a subclass of json::value, wrapping a long.  */

/* Implementation of json::value::print for json::integer_number.  */

void
integer_number::print (pretty_printer *pp,
		       bool formatted ATTRIBUTE_UNUSED) const
{
  char tmp[1024];
  snprintf (tmp, sizeof (tmp), "%ld", m_value);
  pp_string (pp, tmp);
}


/* class json::string, a subclass of json::value.  */

/* json::string's ctor.  */

string::string (const char *utf8)
{
  gcc_assert (utf8);
  m_utf8 = xstrdup (utf8);
  m_len = strlen (utf8);
}

string::string (const char *utf8, size_t len)
{
  gcc_assert (utf8);
  m_utf8 = XNEWVEC (char, len);
  m_len = len;
  memcpy (m_utf8, utf8, len);
}

/* Implementation of json::value::print for json::string.  */

void
string::print (pretty_printer *pp,
	       bool formatted ATTRIBUTE_UNUSED) const
{
  pp_character (pp, '"');
  for (size_t i = 0; i != m_len; ++i)
    {
      char ch = m_utf8[i];
      switch (ch)
	{
	case '"':
	  pp_string (pp, "\\\"");
	  break;
	case '\\':
	  pp_string (pp, "\\\\");
	  break;
	case '\b':
	  pp_string (pp, "\\b");
	  break;
	case '\f':
	  pp_string (pp, "\\f");
	  break;
	case '\n':
	  pp_string (pp, "\\n");
	  break;
	case '\r':
	  pp_string (pp, "\\r");
	  break;
	case '\t':
	  pp_string (pp, "\\t");
	  break;
	case '\0':
	  pp_string (pp, "\\0");
	  break;
	default:
	  pp_character (pp, ch);
	}
    }
  pp_character (pp, '"');
}

/* class json::literal, a subclass of json::value.  */

/* Implementation of json::value::print for json::literal.  */

void
literal::print (pretty_printer *pp,
		bool formatted ATTRIBUTE_UNUSED) const
{
  switch (m_kind)
    {
    case JSON_TRUE:
      pp_string (pp, "true");
      break;
    case JSON_FALSE:
      pp_string (pp, "false");
      break;
    case JSON_NULL:
      pp_string (pp, "null");
      break;
    default:
      gcc_unreachable ();
    }
}


#if CHECKING_P

namespace selftest {

/* Selftests.  */

/* Verify that JV->print () prints EXPECTED_JSON.  */

static void
assert_print_eq (const location &loc,
		 const json::value &jv,
		 bool formatted,
		 const char *expected_json)
{
  pretty_printer pp;
  jv.print (&pp, formatted);
  ASSERT_STREQ_AT (loc, expected_json, pp_formatted_text (&pp));
}

#define ASSERT_PRINT_EQ(JV, FORMATTED, EXPECTED_JSON)	\
  assert_print_eq (SELFTEST_LOCATION, JV, FORMATTED, EXPECTED_JSON)

/* Verify that object::get works as expected.  */

static void
test_object_get ()
{
  object obj;
  value *val = new json::string ("value");
  obj.set ("foo", val);
  ASSERT_EQ (obj.get ("foo"), val);
  ASSERT_EQ (obj.get ("not-present"), NULL);
}

/* Verify that JSON objects are written correctly.  */

static void
test_writing_objects ()
{
  object obj;
  obj.set_string ("foo", "bar");
  obj.set_string ("baz", "quux");
  /* This test relies on json::object writing out key/value pairs
     in key-insertion order.  */
  ASSERT_PRINT_EQ (obj, true,
		   "{\"foo\": \"bar\",\n"
		   " \"baz\": \"quux\"}");
  ASSERT_PRINT_EQ (obj, false,
		   "{\"foo\": \"bar\", \"baz\": \"quux\"}");
}

/* Verify that JSON arrays are written correctly.  */

static void
test_writing_arrays ()
{
  array arr;
  ASSERT_PRINT_EQ (arr, true, "[]");

  arr.append (new json::string ("foo"));
  ASSERT_PRINT_EQ (arr, true, "[\"foo\"]");

  arr.append (new json::string ("bar"));
  ASSERT_PRINT_EQ (arr, true,
		   "[\"foo\",\n"
		   " \"bar\"]");
  ASSERT_PRINT_EQ (arr, false,
		   "[\"foo\", \"bar\"]");
}

/* Verify that JSON numbers are written correctly.  */

static void
test_writing_float_numbers ()
{
  ASSERT_PRINT_EQ (float_number (0), true, "0");
  ASSERT_PRINT_EQ (float_number (42), true, "42");
  ASSERT_PRINT_EQ (float_number (-100), true, "-100");
  ASSERT_PRINT_EQ (float_number (123456789), true, "1.23457e+08");
}

static void
test_writing_integer_numbers ()
{
  ASSERT_PRINT_EQ (integer_number (0), true, "0");
  ASSERT_PRINT_EQ (integer_number (42), true, "42");
  ASSERT_PRINT_EQ (integer_number (-100), true, "-100");
  ASSERT_PRINT_EQ (integer_number (123456789), true, "123456789");
  ASSERT_PRINT_EQ (integer_number (-123456789), true, "-123456789");
}

/* Verify that JSON strings are written correctly.  */

static void
test_writing_strings ()
{
  string foo ("foo");
  ASSERT_PRINT_EQ (foo, true, "\"foo\"");

  string contains_quotes ("before \"quoted\" after");
  ASSERT_PRINT_EQ (contains_quotes, true, "\"before \\\"quoted\\\" after\"");

  const char data[] = {'a', 'b', 'c', 'd', '\0', 'e', 'f'};
  string not_terminated (data, 3);
  ASSERT_PRINT_EQ (not_terminated, true, "\"abc\"");
  string embedded_null (data, sizeof data);
  ASSERT_PRINT_EQ (embedded_null, true, "\"abcd\\0ef\"");
}

/* Verify that JSON literals are written correctly.  */

static void
test_writing_literals ()
{
  ASSERT_PRINT_EQ (literal (JSON_TRUE), true, "true");
  ASSERT_PRINT_EQ (literal (JSON_FALSE), true, "false");
  ASSERT_PRINT_EQ (literal (JSON_NULL), true, "null");

  ASSERT_PRINT_EQ (literal (true), true, "true");
  ASSERT_PRINT_EQ (literal (false), true, "false");
}

/* Verify that nested values are formatted correctly when written.  */

static void
test_formatting ()
{
  object obj;
  object *child = new object;
  object *grandchild = new object;

  obj.set_string ("str", "bar");
  obj.set ("child", child);
  obj.set_integer ("int", 42);

  child->set ("grandchild", grandchild);
  child->set_integer ("int", 1776);

  array *arr = new array;
  for (int i = 0; i < 3; i++)
    arr->append (new integer_number (i));
  grandchild->set ("arr", arr);
  grandchild->set_integer ("int", 1066);

  /* This test relies on json::object writing out key/value pairs
     in key-insertion order.  */
  ASSERT_PRINT_EQ (obj, true,
		   ("{\"str\": \"bar\",\n"
		    " \"child\": {\"grandchild\": {\"arr\": [0,\n"
		    "                                  1,\n"
		    "                                  2],\n"
		    "                          \"int\": 1066},\n"
		    "           \"int\": 1776},\n"
		    " \"int\": 42}"));
  ASSERT_PRINT_EQ (obj, false,
		   ("{\"str\": \"bar\", \"child\": {\"grandchild\":"
		    " {\"arr\": [0, 1, 2], \"int\": 1066},"
		    " \"int\": 1776}, \"int\": 42}"));
}

/* Run all of the selftests within this file.  */

void
json_cc_tests ()
{
  test_object_get ();
  test_writing_objects ();
  test_writing_arrays ();
  test_writing_float_numbers ();
  test_writing_integer_numbers ();
  test_writing_strings ();
  test_writing_literals ();
  test_formatting ();
}

} // namespace selftest

#endif /* #if CHECKING_P */
