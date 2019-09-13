# PEEK - Function that returns the value at the top of the specified slash-delimited stack.
define PEEK
$(lastword $(subst /, ,${1}))
endef

# POP - Function that pops the top value off of the specified slash-delimited stack,
# and returns the updated stack.
define POP
${1:%/$(lastword $(subst /, ,${1}))=%}
endef

# PUSH - Function that pushes a value onto the specified slash-delimited stack,
# and returns the updated stack.
define PUSH
${2:%=${1}/%}
endef
