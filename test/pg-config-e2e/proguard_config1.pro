-keep class com.facebook.redex.test.proguard.Gamma
-keep class com.facebook.redex.test.proguard.Delta$B
-keep class com.facebook.redex.test.proguard.Delta$C
-keep class com.facebook.redex.test.proguard.Delta$C {
  *;
}
-keep class com.facebook.redex.test.proguard.Delta$D
-keep class com.facebook.redex.test.proguard.Delta$D {
  <fields>;
}
-keep class com.facebook.redex.test.proguard.Delta$E
-keep class com.facebook.redex.test.proguard.Delta$E {
  <methods>;
}
-keep class com.facebook.redex.test.proguard.Delta$F
-keep class com.facebook.redex.test.proguard.Delta$F {
  final <fields>;
}
-keep,allowobfuscation class com.facebook.redex.test.proguard.Delta$G
-keep,allowobfuscation class com.facebook.redex.test.proguard.Delta$G {
  *;
}
