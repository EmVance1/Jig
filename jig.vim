syn match   Define     /\w\+\s*=/   contains=Error,Type
syn match   Arrow      /=>\s*\w\+/  contains=Error,Type
syn match   Type       /\w\+/       contained

syn match   Parens     /([^)]\+)/   contains=Function
syn match   Function   /\w\+/       contains=Constant contained
syn match   Constant   /[0-9]\+/    contained
syn match   String     /"\([^\\"]\?\(\\[\\"tn]\)\?\)*"/
syn match   Comment    /#.*\n/

syn keyword Macro        rename module endmod
syn keyword Constant     true false inf _
syn keyword Conditional  if then else match with end
syn keyword Error        START EXIT

