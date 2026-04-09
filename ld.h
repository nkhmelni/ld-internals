// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// ld.h - reverse-engineered layouts for Apple's ld-prime and ld64 linkers
//
// Supports ld-prime (ld-1115.7.3, ld-1221.4, ld-1230.1, ld-1266.8) and ld64 (ld64-820.1).
// All offsets derived from arm64 disassembly and validated against real binaries.
//
// Example - reading atoms and fixups from within the linker process:
//
//   auto lc = ld::layoutConstantsFor(ld::detectVersion());
//   if (!ld::validateBuilder(builder, lc)) return;
//
//   auto *lay = ld::builderGetLayout(builder);
//   auto *sec = ld::findSection(lay, lc, "__DATA", "my_section");
//   if (!sec) return;
//
//   for (auto *ap = ld::sectionAtomsBegin(sec);
//        ap < ld::sectionAtomsEnd(sec); ++ap) {
//       auto span = ld::atomFixups(*ap);
//       for (uint32_t i = 0; i < span.count; ++i)
//           process(span.ptr[i]);
//   }

#ifndef LD_H
#define LD_H

#include "Version.h"
#include "Fixup.h"
#include "Atom.h"
#include "Layout.h"

#endif
