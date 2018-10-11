// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/objects/js-weak-refs-inl.h"

namespace v8 {
namespace internal {

BUILTIN(WeakFactoryConstructor) {
  HandleScope scope(isolate);
  Handle<JSFunction> target = args.target();
  if (args.new_target()->IsUndefined(isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              handle(target->shared()->Name(), isolate)));
  }
  // [[Construct]]
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  Handle<Object> cleanup = args.atOrUndefined(isolate, 1);

  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));

  // TODO(marja): (spec) here we could return an error if cleanup is not a
  // function, if the spec said so.
  Handle<JSWeakFactory> weak_factory = Handle<JSWeakFactory>::cast(result);
  weak_factory->set_cleanup(*cleanup);
  return *weak_factory;
}

BUILTIN(WeakFactoryMakeCell) {
  HandleScope scope(isolate);

  CHECK_RECEIVER(JSWeakFactory, weak_factory, "WeakFactory.makeCell");

  Handle<Object> object = args.atOrUndefined(isolate, 1);
  // TODO(marja): if the type is not an object, throw TypeError. Ditto for
  // SameValue(target, holdings).
  Handle<JSObject> js_object = Handle<JSObject>::cast(object);

  // TODO(marja): Realms.

  Handle<Map> weak_cell_map(isolate->native_context()->js_weak_cell_map(),
                            isolate);

  // Allocate the JSWeakCell object in the old space, because 1) JSWeakCell
  // weakness handling is only implemented in the old space 2) they're
  // supposedly long-living. TODO(marja): Support JSWeakCells in Scavenger.
  Handle<JSWeakCell> weak_cell =
      Handle<JSWeakCell>::cast(isolate->factory()->NewJSObjectFromMap(
          weak_cell_map, TENURED, Handle<AllocationSite>::null()));
  weak_cell->set_factory(*weak_factory);
  weak_cell->set_target(*js_object);
  weak_cell->set_prev(ReadOnlyRoots(isolate).undefined_value());
  weak_cell->set_next(weak_factory->active_cells());
  if (weak_factory->active_cells()->IsJSWeakCell()) {
    JSWeakCell::cast(weak_factory->active_cells())->set_prev(*weak_cell);
  }
  weak_factory->set_active_cells(*weak_cell);
  return *weak_cell;
}

BUILTIN(WeakFactoryCleanupIteratorNext) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSWeakFactoryCleanupIterator, iterator, "next");

  Handle<JSWeakFactory> weak_factory(iterator->factory(), isolate);
  if (!weak_factory->NeedsCleanup()) {
    return *isolate->factory()->NewJSIteratorResult(
        handle(ReadOnlyRoots(isolate).undefined_value(), isolate), true);
  }
  Handle<JSWeakCell> weak_cell_object =
      handle(weak_factory->PopClearedCell(isolate), isolate);

  return *isolate->factory()->NewJSIteratorResult(weak_cell_object, false);
}

}  // namespace internal
}  // namespace v8
