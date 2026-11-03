set(LLVM_LINK_COMPONENTS
  Object
  Support
  )

add_llvm_tool(llvm-elf2tos
  llvm-elf2tos.cpp
  )
