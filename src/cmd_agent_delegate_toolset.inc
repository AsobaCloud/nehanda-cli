static const char *delegate_toolset_override_arg(const opt_parsed_t *opts, int argc, char **argv,
                                                 const char **prompt_io)
{
   const char *toolset = opt_get(opts, "tools");
   if (toolset && !toolset[0])
   {
      for (int i = 0; i + 1 < argc; i++)
      {
         if (!argv[i] || !argv[i + 1])
            continue;
         if (strcmp(argv[i], "--tools") == 0 && i + 1 < argc)
         {
            char resolved[TOOLSET_MAX_TOOLS][TOOLSET_TOOL_MAX], err[TOOLSET_ERROR_MAX] = "";
            if (toolset_resolve_effective(argv[i + 1], resolved, TOOLSET_MAX_TOOLS, err,
                                          sizeof(err)) >= 0)
            {
               toolset = argv[i + 1];
               break;
            }
         }
      }
   }
   if (toolset && toolset[0] && *prompt_io && strcmp(*prompt_io, toolset) == 0)
      *prompt_io = opt_pos(opts, 2);
   return toolset;
}
