-module(fractal).
-export([compute/8]).
-on_load(init/0).

init() ->
  ok = erlang:load_nif("./fractal_nif", 0).

compute(_, _, _, _, _, _, _, _) ->
  exit(nif_library_not_loaded).
