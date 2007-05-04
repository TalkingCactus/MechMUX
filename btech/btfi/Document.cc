/*
 * Implements top-level encoding/decoding for Fast Infoset documents,
 * conforming to X.891 section 12 and annex A, and informed by annex C.
 *
 * Only enough of Fast Infoset is implemented to support saving and loading
 * BattleTech binary databases.  In particular, only the Document, Element, and
 * Attribute types are supported, and additional types as needed to support
 * processing these types.  The objective is to support saving and loading a
 * hierarchical structure, possibly with per-element attributes.
 *
 * However, to ensure backward compatibility, all valid Fast Infoset bitstreams
 * may be decoded, although possibly with the loss of information contained in
 * types beyond the basic three of Document, Element, and Attribute.
 *
 * Implementation restrictions:
 * 7.2.4: restricted-alphabets not written, must be processed.
 * 7.2.5: encoding-algorithms not written, error on read.
 * 7.2.7: additional-data not written, ignored on read.
 * 7.2.13: external-vocabulary not written, error on read.
 * 7.2.16: local-names contains all our identifier strings, while
 *         element-name-surrogates and attribute-name-surrogates contain all
 *         our name surrogates.  other-ncnames, other-uris, attribute-values,
 *         content-character-chunks, and other-strings are not written, but
 *         must be processed. (Some may be ignorable?)
 * 7.2.21: prefixes not written, must be processed. (Ignorable?)
 * 7.2.23: namespace-names contains our namespace name.
 * 7.2.24: [notations] unsupported.
 * 7.2.25: [unparsed entities] unsupported.
 * 7.2.26: [character encoding scheme] must not be set (UTF-8).
 * 7.2.27: [standalone] must not be set (???).
 * 7.2.28: [version] must not be set (???).
 * 7.2.29: [children] must only include the root element, [document element].
 *
 * Update: initial-vocabulary is currently not written, must be processed.
 */

#include "autoconf.h"

#include <cstring>

#include "stream.h"

#include "Exception.hh"
#include "Name.hh"

#include "Document.hh"


namespace BTech {
namespace FI {

namespace {

const char *const BT_NAMESPACE_URI = "http://btonline-btech.sourceforge.net";

bool write_header(FI_OctetStream *);
bool write_trailer(FI_OctetStream *);

#if 0 // defined(FI_USE_INITIAL_VOCABULARY)
bool write_ds_table(FI_OctetStream *, const DS_VocabTable&);
bool write_dn_table(FI_OctetStream *, const DN_VocabTable&);
#endif // FI_USE_INITIAL_VOCABULARY

} // anonymous namespace

Document::Document()
: start_flag (false), stop_flag (false),
  BT_NAMESPACE (namespace_names.getEntry(BT_NAMESPACE_URI))
{
}

void
Document::start()
{
	start_flag = true;
	stop_flag = false;
}

void
Document::stop()
{
	start_flag = false;
	stop_flag = true;
}

void
Document::write(FI_OctetStream *stream)
{
	if (start_flag) {
		// Ensure vocabulary tables are cleared before we start.
		clearVocab();

		// Write document header.
		if (!write_header(stream)) {
			// TODO: Assign an exception for stream errors.
			throw Exception ();
		}

#if 0 // defined(USE_FI_INITIAL_VOCABULARY)
		if (!writeVocab(stream)) {
			// TODO: Assign an exception for stream errors.
			throw Exception ();
		}
#endif // FI_USE_INITIAL_VOCABULARY
	} else if (stop_flag) {
		// Write document trailer.
		if (!write_trailer(stream)) {
			// TODO: Assign an exception for stream errors.
			throw Exception ();
		}

		// Clear vocabulary tables, to save some memory.  If we're
		// caching any entries, they'll remain interned, so we won't
		// constantly be reallocating the same entries.  This will
		// reset their vocabulary indexes, however, as intended.
		clearVocab();
	} else {
		throw IllegalStateException ();
	}
}

void
Document::read(FI_OctetStream *stream)
{
	if (start_flag) {
		// Ensure vocabulary tables are cleared before we start.
		clearVocab();

		// Try to read document header.

		// Determine next child type.
		setNext(0); // XXX: DEBUG
	} else if (stop_flag) {
		// Try to read document trailer.


		// Clear vocabulary tables, to save some memory.  If we're
		// caching any entries, they'll remain interned, so we won't
		// constantly be reallocating the same entries.  This will
		// reset their vocabulary indexes, however, as intended.
		clearVocab();
	} else {
		throw IllegalStateException ();
	}
}

// Namespaces in XML 1.0 (Second Edition)
// http://www.w3.org/TR/2006/REC-xml-names-20060816
//
// Section 6.2, paragraph 2:
//
// A default namespace declaration applies to all unprefixed element names
// within its scope. Default namespace declarations do not apply directly to
// attribute names; the interpretation of unprefixed attributes is determined
// by the element on which they appear.

const DN_VocabTable::TypedEntryRef
Document::getElementName(const char *name)
{
	const Name element_name (local_names.getEntry(name), BT_NAMESPACE);
	return element_name_surrogates.getEntry(element_name);
}

const DN_VocabTable::TypedEntryRef
Document::getAttributeName(const char *name)
{
	const Name attribute_name (local_names.getEntry(name));
	return attribute_name_surrogates.getEntry(attribute_name);
}


void
Document::clearVocab()
{
	restricted_alphabets.clear();
	encoding_algorithms.clear();

	prefixes.clear();
	namespace_names.clear();
	local_names.clear();

	other_ncnames.clear();
	other_uris.clear();

	attribute_values.clear();
	content_character_chunks.clear();
	other_strings.clear();

	element_name_surrogates.clear();
	attribute_name_surrogates.clear();
}

#if 0 // defined(FI_USE_INITIAL_VOCABULARY)
bool
Document::writeVocab(FI_OctetStream *stream)
{
	// Write padding (C.2.5: 000).
	// Write optional component presence flags (C.2.5.1: 00001 ?00000??).
	FI_Octet *w_buf = fi_get_stream_write_buffer(stream, 2);
	if (!w_buf) {
		return false;
	}

	w_buf[0] = FI_BIT_8 /* namespace-names present */;
	w_buf[1] = (local_names.size() ? FI_BIT_1 : 0)
	           | (element_name_surrogates.size() ? FI_BIT_7 : 0)
	           | (attribute_name_surrogates.size() ? FI_BIT_8 : 0);

	// Write namespace-names (C.2.5.3).
	if (!write_ds_table(stream, namespace_names)) {
		return false;
	}

	// Write local-names (C.2.5.3)?
	if (local_names.size() && !write_ds_table(stream, local_names)) {
		return false;
	}

	// Write element-name-surrogates (C.2.5.5)?
	if (element_name_surrogates.size()
	    && !write_dn_table(stream, element_name_surrogates)) {
		return false;
	}

	// Write attribute-name-surrogates (C.2.5.5)?
	if (attribute_name_surrogates.size()
	    && !write_dn_table(stream, attribute_name_surrogates)) {
		return false;
	}

	return true;
}
#endif // !FI_USE_INITIAL_VOCABULARY


namespace {

//
// Document serialization subroutines.
//

const char *
get_xml_decl(int version, int standalone)
{
	static const char *const xml_decl[] = {
		"<?xml encoding='finf'?>",
		"<?xml encoding='finf' standalone='no'?>",
		"<?xml encoding='finf' standalone='yes'?>",
		"<?xml version='1.0' encoding='finf'?>",
		"<?xml version='1.0' encoding='finf' standalone='no'?>",
		"<?xml version='1.0' encoding='finf' standalone='yes'?>",
		"<?xml version='1.1' encoding='finf'?>",
		"<?xml version='1.1' encoding='finf' standalone='no'?>",
		"<?xml version='1.1' encoding='finf' standalone='yes'?>"
	};

	return xml_decl[3 * version + standalone];
}

bool
write_header(FI_OctetStream *stream)
{
	// Decide which XML declaration to use.
	const char *xml_decl = get_xml_decl(0 /* no version */,
	                                    0 /* no standalone */);

	size_t xml_decl_len = strlen(xml_decl);

	// Reserve space.
	FI_Octet *w_buf = fi_get_stream_write_buffer(stream, xml_decl_len + 5);
	if (!w_buf) {
		return false;
	}

	// Write XML declaration.
	memcpy(w_buf, xml_decl, xml_decl_len);
	w_buf += xml_decl_len;

	// Write identification (12.6: 11100000 00000000).
	w_buf[0] = FI_BITS(1,1,1,0,0,0,0,0);
	w_buf[1] = FI_BITS(0,0,0,0,0,0,0,0);

	// Write Fast Infoset version number (12.7: 00000000 00000001).
	w_buf[2] = FI_BITS(0,0,0,0,0,0,0,0);
	w_buf[3] = FI_BITS(0,0,0,0,0,0,0,1);

	// Write padding (12.8: 0).
#if 0 // defined(FI_USE_INITIAL_VOCABULARY)
	// Write optional component presence flags (C.2.3: 0100000).
	w_buf[4] = FI_BITS(,0,1 /* initial-vocabulary not present */,0,0,0,0,0);
#else // !FI_USE_INITIAL_VOCABULARY
	// Write optional component presence flags (C.2.3: 0000000).
	w_buf[4] = FI_BITS(,0,0 /* initial-vocabulary not present */,0,0,0,0,0);
#endif // !FI_USE_INITIAL_VOCABULARY

	return true;
}

bool
write_trailer(FI_OctetStream *stream)
{
	FI_Octet *w_buf;

	// Serialize document trailer.
	switch (fi_get_stream_num_bits(stream)) {
	case 0:
		// Write termination (C.2.12: 1111).
		// 12.11: End on the 4th bit of an octet.
		w_buf = fi_get_stream_write_buffer(stream, 1);
		if (!w_buf) {
			return false;
		}

		w_buf[0] = FI_BITS(1,1,1,1,,,,);
		break;

	case 4:
		// Write termination (C.2.12: 1111).
		// 12.11: End on the 8th bit of an octet.
		if (!fi_write_stream_bits(stream, 4, FI_BITS(1,1,1,1,,,,))) {
			return false;
		}
		break;

	default:
		// Shouldn't happen.
		return false;
	}

	return true;
}

#if 0 // defined(FI_USE_INITIAL_VOCABULARY)
// C.2.5.3: Identifier string vocabulary tables.
bool
write_ds_table(FI_OctetStream *stream, const DS_VocabTable& vocab)
{
	// Write string count (C.21).
	if (!write_length_sequence_of(stream,
	                              vocab.size() - vocab.last_builtin)) {
		return false;
	}

	// Write items.
	for (FI_VocabIndex ii = vocab.first_added; ii <= vocab.size(); ii++) {
		// Write item padding (C.2.5.3: 0).
		if (!fi_write_stream_bits(stream, 1, 0)) {
			return false;
		}

		// Write item (C.22).
		if (!write_non_empty_string_bit_2(stream, vocab[ii])) {
			return false;
		}
	}

	return true;
}

// C.2.5.5: Name surrogate vocabulary tables.
bool
write_dn_table(FI_OctetStream *stream, const DN_VocabTable& vocab)
{
	// Write name surrogate count (C.21).
	if (!write_length_sequence_of(stream, vocab.size())) {
		return false;
	}

	// Write items.
	for (FI_VocabIndex ii = 1; ii <= vocab.size(); ii++) {
		// Write item padding (C.2.5.5: 000000).
		if (!fi_write_stream_bits(stream, 6, 0)) {
			return false;
		}

		// Write item (C.16).
		if (!write_name_surrogate(stream, vocab[ii])) {
			return false;
		}
	}

	return true;
}
#endif // FI_USE_INITIAL_VOCABULARY

} // anonymous namespace

} // namespace FI
} // namespace BTech