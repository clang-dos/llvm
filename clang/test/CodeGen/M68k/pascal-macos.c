// RUN: %clang_cc1 -triple m68k-apple-macos -emit-llvm %s -o - | FileCheck %s

// Classic Mac OS Toolbox declarations: "pascal" selects the Pascal calling
// convention and raw_inline gives the trap words a call emits.

typedef struct { short v, h; } Point;
typedef struct { short top, left, bottom, right; } Rect;

pascal void InitGraf(void *p) __attribute__((raw_inline(0xA86E)));
pascal unsigned char Button(void) __attribute__((raw_inline(0xA974)));
pascal unsigned char PtInRect(Point pt, const Rect *r)
    __attribute__((raw_inline(0xA8AD)));
pascal long TwoWords(short) __attribute__((raw_inline(0x3F3C, 0x0001)));
pascal void TakesRect(Rect r);
pascal Point ReturnsPoint(void);

// CHECK-LABEL: define{{.*}} void @f(
// CHECK: call m68k_pascalcc void @InitGraf(ptr noundef %{{.*}})
// CHECK: call m68k_pascalcc zeroext i8 @Button()
// CHECK: call m68k_pascalcc zeroext i8 @PtInRect(i32 %{{.*}}, ptr noundef %{{.*}})
// CHECK: call m68k_pascalcc i32 @TwoWords(i16 noundef signext 7)
// CHECK: call m68k_pascalcc void @TakesRect(ptr {{.*}}%byval-temp)
// CHECK: call m68k_pascalcc i32 @ReturnsPoint()
void f(void *p, Point pt, Rect *r) {
  InitGraf(p);
  Button();
  PtInRect(pt, r);
  TwoWords(7);
  TakesRect(*r);
  ReturnsPoint();
}

// CHECK-DAG: declare m68k_pascalcc void @InitGraf(ptr noundef) #[[IG:[0-9]+]]
// CHECK-DAG: declare m68k_pascalcc i32 @TwoWords(i16 noundef signext) #[[TW:[0-9]+]]

// A Pascal function defined in C, such as a Toolbox callback.
// CHECK-LABEL: define{{.*}} m68k_pascalcc signext i16 @Callback(i16 noundef signext %a, i32 noundef %b)
pascal short Callback(short a, long b) { return a + (short)b; }

// CHECK-DAG: attributes #[[IG]] = {{.*}}"m68k-inline"="A86E"
// CHECK-DAG: attributes #[[TW]] = {{.*}}"m68k-inline"="3F3C,0001"
