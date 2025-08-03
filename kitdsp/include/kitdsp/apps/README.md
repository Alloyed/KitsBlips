# apps

each app represents a fully abstracted effect that can be used directly or plugged into a larger instrument/effect chain.

rules:

- have a struct Config that you can directly manipulate
- have a per sample Process() method.
- if valuable, have an multi-sample Process(etl::span<>) override. if used then config is assumed to be static for the entire buffer
