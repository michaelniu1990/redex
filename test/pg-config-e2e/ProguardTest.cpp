/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <cstdint>
#include <cstdlib>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <string>

#include "DexClass.h"
#include "DexInstruction.h"
#include "DexLoader.h"
#include "Match.h"
#include "ProguardConfiguration.h"
#include "ProguardMap.h"
#include "ProguardMatcher.h"
#include "ProguardParser.h"
#include "ReachableClasses.h"
#include "RedexContext.h"

/**
The objective of these tests are to make sure the ProGuard rules are
properly applied to a set of test classes. The incomming APK is currently
already processed by ProGuard. This test makes sure the expected classes
and methods are present (or absent) as required and performs checks on the
Redex ProGuard rule matcher to make sure the ProGuard rules were properly
interpreted.
**/

ProguardMap* proguard_map;

DexClass* find_class_named(const DexClasses& classes, const std::string name) {
  auto mapped_search_name = std::string(proguard_map->translate_class(name));
  auto it = std::find_if(
      classes.begin(), classes.end(), [&mapped_search_name](DexClass* cls) {
        return mapped_search_name == std::string(cls->c_str());
      });
  if (it == classes.end()) {
    return nullptr;
  } else {
    return *it;
  }
}

DexMethod* find_vmethod_named(const DexClass* cls, const std::string name) {
  auto vmethods = cls->get_vmethods();
  auto mapped_search_name = std::string(proguard_map->translate_method(name));
  auto it = std::find_if(
      vmethods.begin(), vmethods.end(), [&mapped_search_name](DexMethod* m) {
        return (mapped_search_name == std::string(m->c_str()) ||
                (mapped_search_name == proguard_name(m)));
      });
  return it == vmethods.end() ? nullptr : *it;
}

DexField* find_instance_field_named(const DexClass* cls, const char* name) {
  auto fields = cls->get_ifields();
  auto mapped_search_name = std::string(proguard_map->translate_field(name));
  auto it = std::find_if(
      fields.begin(), fields.end(), [&mapped_search_name](DexField* f) {
        return (mapped_search_name == std::string(f->c_str()) ||
                (mapped_search_name == proguard_name(f)));
      });
  return it == fields.end() ? nullptr : *it;
}

bool class_has_been_renamed(const std::string class_name) {
  auto mapped_name = std::string(proguard_map->translate_class(class_name));
  return class_name != mapped_name;
}

/**
 * Ensure the ProGuard test rules are properly applied.
 */
TEST(ProguardTest, assortment) {
  g_redex = new RedexContext();

  const char* dexfile = std::getenv("pg_config_e2e_dexfile");
  ASSERT_NE(nullptr, dexfile);

  std::vector<DexClasses> dexen;
  dexen.emplace_back(load_classes_from_dex(dexfile));
  DexClasses& classes = dexen.back();

  // Load the Proguard map
  const char* mapping_file = std::getenv("pg_config_e2e_mapping");
  ASSERT_NE(nullptr, mapping_file);
  proguard_map = new ProguardMap(std::string(mapping_file));

  const char* configuraiton_file = std::getenv("pg_config_e2e_pgconfig");
  ASSERT_NE(nullptr, configuraiton_file);
  redex::ProguardConfiguration pg_config;
  redex::proguard_parser::parse_file(configuraiton_file, &pg_config);
  ASSERT_TRUE(pg_config.ok);

  Scope scope = build_class_scope(dexen);
  process_proguard_rules(pg_config, proguard_map, scope);

  { // Alpha is explicitly used and should not be deleted.
    auto alpha =
        find_class_named(classes, "Lcom/facebook/redex/test/proguard/Alpha;");
    ASSERT_NE(alpha, nullptr);
    ASSERT_FALSE(keep(alpha));
    ASSERT_FALSE(keepclassmembers(alpha));
    ASSERT_FALSE(keepclasseswithmembers(alpha));
  }

  // Beta is not used and should not occur in the input.
  ASSERT_EQ(
      find_class_named(classes, "Lcom/facebook/redex/test/proguard/Beta;"),
      nullptr);

  { // Gamma is not used anywhere but is kept by the config.
    auto gamma =
        find_class_named(classes, "Lcom/facebook/redex/test/proguard/Gamma;");
    ASSERT_NE(gamma, nullptr);
    ASSERT_TRUE(keep(gamma));
    ASSERT_FALSE(keepclassmembers(gamma));
    ASSERT_FALSE(keepclasseswithmembers(gamma));
  }

  { // Inner class Delta.A should be removed.
    auto delta_a =
        find_class_named(classes, "Lcom/facebook/redex/test/proguard/Delta$A;");
    ASSERT_EQ(delta_a, nullptr);
  }

  { // Inner class Delta.B is preserved by a keep directive.
    auto delta_b =
        find_class_named(classes, "Lcom/facebook/redex/test/proguard/Delta$B;");
    ASSERT_NE(delta_b, nullptr);
    ASSERT_TRUE(keep(delta_b));
  }

  { // Inner class Delta.C is kept.
    auto delta_c =
        find_class_named(classes, "Lcom/facebook/redex/test/proguard/Delta$C;");
    ASSERT_NE(delta_c, nullptr);
    ASSERT_TRUE(keep(delta_c));
    // Make sure its fields and methods have been kept by the "*;" directive.
    auto iField = find_instance_field_named(delta_c, "i");
    ASSERT_NE(iField, nullptr);
    auto iValue = find_vmethod_named(delta_c, "iValue");
    ASSERT_NE(iValue, nullptr);
  }

  { // Inner class Delta.D is kept.
    auto delta_d =
        find_class_named(classes, "Lcom/facebook/redex/test/proguard/Delta$D;");
    ASSERT_NE(delta_d, nullptr);
    ASSERT_TRUE(keep(delta_d));
    // Make sure its fields are kept by "<fields>" but not its methods.
    auto iField = find_instance_field_named(delta_d, "i");
    ASSERT_NE(iField, nullptr);
    auto iValue = find_vmethod_named(delta_d, "iValue");
    ASSERT_EQ(iValue, nullptr);
  }

  { // Inner class Delta.E is kept.
    auto delta_e =
        find_class_named(classes, "Lcom/facebook/redex/test/proguard/Delta$E;");
    ASSERT_NE(delta_e, nullptr);
    ASSERT_TRUE(keep(delta_e));
    // Make sure its methods are kept by "<methods>" but not its fields.
    auto iField = find_instance_field_named(delta_e, "i");
    ASSERT_EQ(iField, nullptr);
    auto iValue = find_vmethod_named(delta_e, "iValue");
    ASSERT_NE(iValue, nullptr);
  }

  { // Inner class Delta.F is kept and its final fields are kept.
    auto delta_f =
        find_class_named(classes, "Lcom/facebook/redex/test/proguard/Delta$F;");
    ASSERT_NE(delta_f, nullptr);
    ASSERT_TRUE(keep(delta_f));
    // Make sure only the final fields are kept.
    // wombat is not a final field, so it should not be kept.
    auto wombatField = find_instance_field_named(delta_f, "wombat");
    ASSERT_EQ(wombatField, nullptr);
    // numbat is a final field so it should be kept
    auto numbatField = find_instance_field_named(delta_f, "numbat");
    ASSERT_NE(numbatField, nullptr);
    // The numbatValue method should not be kept.
    auto numbatValue = find_vmethod_named(delta_f, "numbatValue");
    ASSERT_EQ(numbatValue, nullptr);
  }

  { // Inner class Delta.G is kept.
    auto delta_g =
        find_class_named(classes, "Lcom/facebook/redex/test/proguard/Delta$G;");
    ASSERT_NE(delta_g, nullptr);
    ASSERT_TRUE(keep(delta_g));
    ASSERT_TRUE(allowobfuscation(delta_g));
    ASSERT_TRUE(
        class_has_been_renamed("Lcom/facebook/redex/test/proguard/Delta$G;"));
    // Make sure its fields and methods have been kept by the "*;" directive.
    auto wombatField = find_instance_field_named(
        delta_g, "Lcom/facebook/redex/test/proguard/Delta$G;.wombat:I");
    ASSERT_NE(wombatField, nullptr);
    auto wombatValue = find_vmethod_named(
        delta_g, "Lcom/facebook/redex/test/proguard/Delta$G;.wombatValue()I");
    ASSERT_NE(wombatValue, nullptr);
  }

  delete g_redex;
}
