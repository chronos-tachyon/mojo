# Event Loops and Asynchronous Programming

**NOTE**: Most users will be interested in the `event::Manager` API.

**NOTE**: These APIs are frequently used with [the I/O APIs](../io/README.md).

## event/callback.h
## event/task.h
## event/dispatcher.h

RC.  Defines the following:

* `event::Callback`, a base class for oneshot callback functions;
* `event::Task`, a class that allows observers to watch an operation; and
* `event::Dispatcher`, a base class for running callbacks.

Three basic models of `event::Dispatcher` are provided: **inline** dispatchers
run their callbacks immediately; **async** dispatchers defer their callbacks
until the next turn of the event loop; and **threaded** dispatchers run their
callbacks ASAP on a thread pool.

Quick example:

    // Create a new asynchronous dispatcher.
    event::DispatcherPtr dispatcher;
    event::DispatcherOptions do;
    do.set_type(event::DispatcherType::async_dispatcher);
    CHECK_OK(event::new_dispatcher(&dispatcher, do));

    // Start an asynchronous operation.
    event::Task task;
    operation(&task, dispatcher);

    // Schedules an action to run when the operation completes.
    bool done = false;
    task->on_finish(event::callback([&task, &done] {
      CHECK_OK(task.result());
      done = true;
      return base::Result();
    });

    // Spin the event loop.
    while (!done) dispatcher->donate(false);

## event/set.h
## event/poller.h

RC.  Provides `event::Poller`, a base class that abstracts over event polling
techniques, and helper class `event::Set`, a value type representing a set of
events.

## event/data.h
## event/handler.h
## event/manager.h

RC.  Defines the following:

* `event::Data`, an aggregate struct containing data about an event;
* `event::Handler`, a base class for multishot callback functions;
* `event::Manager`, a class that integrates polling and dispatching;
* `event::FileDescriptor`, `event::Signal`, `event::Timer`, and
  `event::Generic`, each of which binds an event to an `event::Handler`.

Example:

    // Create a new asynchronous event manager.
    event::Manager m;
    event::ManagerOptions mo;
    mo.set_async_mode();
    CHECK_OK(event::new_manager(&m, mo));

    // Set up a non-blocking socket.
    int fdnum = accept4(..., SOCK_NONBLOCK);
    base::FD fd = base::wrapfd(fdnum);

    // Operate on socket asynchronously.
    event::FileDescriptor evt;
    bool done = false;
    auto closure = [&](event::Data data) {
      if (data.events.error()) {
        // Got an error from the remote end!
        CHECK_OK(evt.disable());  // No more handler callbacks, please.
        CHECK_OK(fd->close());
        return;
      }
      if (data.events.readable()) {
        // Socket has data to read!
        ...;  // Call read(2) in a loop until -1, errno=EAGAIN.
      }
      if (data.events.writable()) {
        // Socket can accept writes!
        ...;  // Call write(2) in a loop until -1, errno=EAGAIN.
      }
    }
    CHECK_OK(m.fd(&evt, fd, event::Set::all_bits(), event::handler(closure)));

    // Spin the event loop.
    while (!done) m.donate(false);

