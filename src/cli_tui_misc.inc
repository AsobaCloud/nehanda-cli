static char *join_message_args(int argc, char **argv)
{
   size_t len = 0;
   for (int i = 0; i < argc; i++)
      len += strlen(argv[i]) + 1;
   char *out = malloc(len + 1);
   if (!out)
      return NULL;
   out[0] = '\0';
   for (int i = 0; i < argc; i++)
   {
      if (i)
         strcat(out, " ");
      strcat(out, argv[i]);
   }
   return out;
}
