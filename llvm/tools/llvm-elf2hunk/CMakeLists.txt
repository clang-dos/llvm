set(LLVM_LINK_COMPONENTS
  Object
  Support
  )

add_llvm_tool(llvm-elf2hunk
  llvm-elf2hunk.cpp
  )
