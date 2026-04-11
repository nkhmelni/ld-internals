// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// ld.h - reverse-engineered layouts for Apple's ld-prime and ld64 linkers.
//
// Supports ld-prime 1115.7.3 / 1221.4 / 1230.1 / 1266.8 and ld64-820.1.
// Offsets are derived from arm64/x86_64 disassembly and cross-checked
// against real linker binaries.
//
// Minimal usage pattern:
//
//   auto lc = ld::layoutConstantsFor(ld::detectVersion());
//   if (!ld::validateBuilder(builder, lc)) return;
//
//   const void *lay = ld::builderGetLayout(builder);
//   const void *sec = ld::findSection(lay, lc, "__DATA", "my_section");
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
#include "Primitives.h"
#include "Mach.h"
#include "LinkEdit.h"
#include "Fixup.h"
#include "Atom.h"
#include "Options.h"
#include "Consolidator.h"
#include "Layout.h"

#endif
