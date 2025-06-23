# C++ API Guidelines

**WIP** - *please feel free to improve*

Intended for Firebase APIs, but also applicable to any C++ or Game APIs.

# Code Style

Please comply with the
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
as much as possible. Refresh your memory of this document before you start :)

# C++ API Design

### Don't force any particular usage pattern upon the client.

C++ is a huge language, with a great variety of ways in which things can be
done, compared to other languages. As a consequence, C++ projects can be very
particular about what features of the language they use or don't use, how they
represent their data, and structure their code.

An API that forces the use of a feature or structure the client doesn't use
will be very unwelcome. A good API uses only the simplest common denominator
data types and features, and will be useable by all. This can generally be done
with minimal impact on your API’s simplicity, or at least should form the
baseline API.

Examples of typical Do's and Don'ts:

* Don't force the use of a particular data structure to supply or receive data.
  Typical examples:
    * `std::vector<T>`: If the client doesn't have the data already in a
       `std::vector` (or only wants to use part of a vector), they are forced
       to copy/allocate a new one, and C++ programmers don't like unnecessary
       copies/allocations.
       Instead, your primary interface should always take a
       `(const T *, size_t)` instead. You can still supply an optional helper
       method that takes a `std::vector<T> &` and then calls the former if
       you anticipate it to be called very frequently.
    * `std::string`: Unlike Java, these things aren't pooled, they're mutable
       and copied. A common mistake is to take a `const std::string &` argument,
       which forces all callers that supply a `const char *` to go thru a
       strlen+malloc+copy that is possibly of no use to the callee. Prefer to
       take a `const char *` instead for things that are names/identifiers,
       especially if they possibly are compile-time constant.  If you're
       unsetting a string property, prefer to pass nullptr rather than an
       empty string. (There are
       [COW implementations](https://en.wikipedia.org/wiki/Copy-on-write),
       but you can't rely on that).
    * `std::map<K,V>`: This is a costly data structure involving many
       allocations. If all you wanted is for the caller to supply a list of
       key/value pairs, take a `const char **` (yes, 2 stars!). Or
       `const SimpleStruct *` instead, which allows the user to create this data
       statically.
* Per-product configuration should be accomplished using an options struct
  passed to the library's `firebase::<library>::Initialize` function. Default
  options should be provided by the options struct's default constructor. The
  `Initialize` function should be overloaded with a version that does not take
  the options struct (which is how the Google style guide prefers that we pass
  default parameters).

  For example,

```c++
    struct LibraryOptions {
      LibraryOptions() : do_the_thing(false) {}

      bool do_the_thing;
    };

    InitResult Initialize(const App& app) {
      return Initialize(app, LibraryOptions());
    }
    InitResult Initialize(const App& app, const LibraryOptions& options);
```

* Don't make your client responsible for data you allocate or take ownership
  of client data. Typical C++ APIs are written such that both the client
  and the library own their own memory, and they take full responsibility for
  managing it, regardless of what the other does. Any data exchanged is
  typically done through weak references and copies.
  An exception may be a file loading API where buffers exchanged may be really
  big. If you are going to pass ownership, make this super obvious in all
  comments and documentation (C++ programmers typically won't expect it),
  and document which function should be used to free the data (free, delete,
  or a custom one).
  Alternatively, a simple way to pass ownership of a large new buffer to the
  client is to ask the client to supply a std::string *, which you then
  resize(), and write directly into its owned memory. This somewhat violates
  the rule about use of std::string above, though.

* Don't use exceptions. This one is worth mentioning separately. Though
  exceptions are great in theory, in C++ hardly any libraries use them, and
  many code-bases disallow them entirely. They also require the use of RTTI
  which some environments turn off. Oh, yes, also don't use RTTI.

* Go easy on templates when possible. Yes, they make your code more general,
  but they also pull a lot of implementation detail into your API, lengthen
  compile times and bloat binaries. In C++ they are glorified macros, so they
  result in hard to understand errors, and can make correct use of your API
  harder to understand.

* Utilize C++11 features where appropriate. This project has adopted C++11,
  and features such as `std::unique_ptr`, `std::shared_ptr`, `std::make_unique`,
  and `std::move` are encouraged to improve code safety and readability.
  However, avoid features from C++14 or newer standards.

* Go easy on objectifying everything, and prefer value types. In languages
  like Java it is common to give each "concept" your API deals with its own
  class, such that methods on it have a nice home. In C++ this isn't always
  desirable, because objects need to be managed, stored and allocated, and you
  run into ownership/lifetime questions mentioned above. Instead:

    * For simple data, prefer their management to happen in the parent class
      that owns them. Actions on them are methods in the parent. If at all
      possible, prefer not to refer to them by pointer/reference (which
      creates ownership and lifetime issues) but by index/id, or string
      if not performance sensitive (for example, when referring to file
      resources, since the cost of loading a file dwarfs the cost of a string
      lookup).

    * If you must create objects, and objects are not heavyweight (only scalars
      and non-owned pointers), make use of these objects by value (return by
      value, receive by const reference). This makes ownership and lifetime
      management trivial and efficient.

* If at all possible, don't depend on external libraries. C++ compilation,
  linking, dependency management, testing (especially cross platform) are
  generally way harder than any other language. Every dependency is a potential
  source of build complexity, conflicts, efficiency issues, and in general more
  dependencies means less adoption.

    * Don't pull in large libraries (e.g. BOOST) just for your convenience,
      especially if their use is exposed in headers.

    * Only use external libraries that have hard to replicate essential
      functionality (e.g. compression, serialization, image loading, networking
      etc.). Make sure to only access them in implementation files.

    * If possible, make a dependency optional, e.g. if what your API does
      benefits from compression, make the client responsible for doing so,
      or add an interface for it. Add sample glue code or an optional API for
      working with the external library that is by default off in the build
      files, and can be switched on if desired.

* Take cross-platform-ness seriously: design the API to work on ALL platforms
  even if you don't intend to supply implementations for all. Hide platform
  issues in the implementation. Don't ever include platform specific headers in
  your own headers. Have graceful fallback for platforms you don't support, such
  that some level of building / testing can happen anywhere.

* If your API is meant to be VERY widespread in use, VERY general, and very
  simple (e.g. a compression API), consider making at least the API (if not all
  of it) in C, as you'll reach an even wider audience. C has a more consistent
  ABI and is easier to access from a variety of systems / languages. This is
  especially useful if the library implementation is intended to be provided in
  binary.

* Be careful not to to use idioms from other languages that may be foreign to
  C++.

    * An example of this is a "Builder" API (common in Java). Prefer to use
      regular constructors, with additional set_ methods for less common
      parameters if the number of them gets overwhelming.

* Do not expose your own UI to the user as part of an API. Give the developer
  the data to work with, and let them handle displaying it to the user in the
  way they see fit.

    * Rare exceptions can be made to this rule on a case-by-case basis. For
      example, authentication libraries may need to display a sign-in UI for the
      user to enter their credentials. Your API may work with data owned by
      Google or by the user (e.g. the user's contacts) that we don't want to
      expose to the app; in those cases, it is appropriate to expose a UI (but
      to limit the scope of the UI to the minimum necessary).

    * In these types of exceptional cases, the UI should be in an isolated
      component, separate from the rest of the API. We do allow UIs to be
      exposed to the user UI-specific libraries, e.g. FirebaseUI, which should
      be open-source so developers can apply any customizations they need.

# Game API Design

### Performance matters

Most of this is already encoded in C++ API design above, but it bears repeating:
C++ game programmers can be more fanatic about performance than you expect.

It is easy to add a layer of usability on top of fast code, it is very hard to
impossible to "add performance" to an API that has performance issues baked into
its design.

### Don't rely on state persisting for longer than one frame.

Games have an interesting program structure very unlike apps or web pages: they
do all processing (and rendering) of almost all functionality of the game within
a *frame* (usually 1/60th of a second), and then start anew for the next frame.

It is common for all or part of the state of a game to be wiped out from one
frame to the next (e.g when going into the menu, loading a new level, starting a
cut-scene..).

The consequence of this is that the state kept between frames is the only record
of what is currently going on, and that managing this state is a source of
complexity, especially when part of it is reflected in external code:

* Prefer API design that is stateless, or if it is stateful, is so only within a
  frame (i.e. between the start of the frame and the start of the next one).
  This really simplifies the client's use of your API: they can't forget to
  "reset" your API's state whenever they change state themselves.

* Prefer not to use cross-frame callbacks at all (non-escaping callbacks are
  fine). Callbacks can be problematic in other contexts, but they're even more
  problematic in games. Since they will execute at a future time, there's no
  guarantee that the state that was there when the callback started will still
  be there. There's no easy way to robustly "clear" pending callbacks that don't
  make sense anymore when state changes. Instead, make your API based on
  *polling*.
  Yes, everyone learned in school that polling is bad because it uses CPU, but
  that's what games are based on anyway: they check a LOT of things every frame
  (and only at 60hz, which is very friendly compared to busy-wait polling). If
  your API can be in various states of a state machine (e.g. a networking based
  API), make sure the client can poll the state you're in. This can then easily
  be translated to user feedback.
  If you have to use asynchronous callbacks, see the section on async operations
  below.

* Be robust to the client needing to change state. If work done in your API
  involves multiple steps, and the client never gets to the end of those steps
  before starting a new sequence, don't be "stuck", but deal with this
  gracefully. If the game's state got reset, it will have no record of what it
  was doing before. Try to not make the client responsible for knowing what it
  was doing.

* Interaction with threading:

    * If you are going to use threading at all, make sure the use of that is
      internal to the library, and any issues of thread-safety don't leak into
      the client. Allow what appears to be synchronous access to a possibly
      asynchronous implementation. If the asynchronous nature will be
      externally visible, see the section on async operations below.

    * Games are typically hard to thread (since it’s hard to combine with its
      per-frame nature), so the client typically should have full control over
      it: it is often better to make a fully synchronous single-threaded library
      and leave threading it to the client. Do not try to make your API itself
      thread-safe, as your API is unlikely the threading boundary (if your
      client is threaded, use of your library is typically isolated to one
      thread, and they do not want to pay for synchronization cost they don't
      use).

    * When you do spin up threads to reduce a workload, it is often a good idea
      to do that once per frame, as avoid the above mentioned state based
      problems, and while starting threads isn't cheap, you may find it not a
      problem to do 60x per second. Alternatively you can pool them, and make
      sure you have an explicit way to wait for their idleness at the end of a
      frame.

* Games typically use their own memory allocator (for efficiency, but also to be
  able to control and budget usage on memory constrained systems). For this
  reason, most game APIs tend to provide allocation hooks that will be used for
  all internal allocation. This is even more important if you wish to be able to
  transfer ownership of memory.

* Generally prefer solutions that are low on total memory usage. Games are
  always constrained on memory, and having your game be killed by the OS because
  the library you use has decided it is efficient to cache everything is
  problematic.

    * Prefer to recompute values when possible.

    * When you do cache, give the client control over total memory used for
      this purpose.

    * Your memory usage should be predictable and ideally have no peaks.

# Async Operations

### Application Initiated Async Operations

* Use the Future / State Pattern.
* Add a `*LastResult()` method for each async operation method to allow the caller
  to poll and not save state.

e.g.

```c++
    // Start async operation.
    Future<SignInWithCrendentialResult> SignInWithCredential(...);
    // Get the result of the pending / last async operation for the method.
    Future<SignInWithCrendentialResult> SignInWithCredentialLastResult();

    Usage examples:
    // call and register callback
    auto& result = SignInWithCredential();
    result.set_callback([](result) { if (result == kComplete) { do_something_neat(); wake_up(); } });
    // wait

    // call and poll #1 (saving result)
    auto& result = SignInWithCredential();
    while (result.value() != kComplete) {
      // wait
    }

    // call and poll #2 (result stored in API)
    SignInWithCredential();
    while (SignInWithCredentialLastResult().value() != kComplete) {
    }
```

### API Initiated Async Event Handling

* Follow the
  [listener / observer pattern](https://en.wikipedia.org/wiki/Observer_pattern)
  for API initiated (i.e where the caller doesn't initiate the event)
  async events.
* Provide a queued interface to allow users to poll for events.

e.g.

```c++
    class GcmListener {
     public:
      virtual void OnDeletedMessage() {}
      virtual void OnMessageReceived(const MessageReceivedData* data) {}
    };

    class GcmListenerQueue : private GcmListener {
     public:
      enum EventType {
        kEventTypeMessageDeleted,
        kEventTypeMessageReceived,
      };

      struct Event {
        EventType type;
        MessageReceivedData data;  // Set when type == kEventTypeMessageReceived
      };

      // Returns true when an event is retrieved from the queue.
      bool PollEvent(Event *event);
    };

    // Wait for callbacks
    class MyListener : public GcmListener {
     public:
      virtual void OnDeletedMessage() { /* do stuff */ }
      virtual void OnMessageReceived() { /* display message */ }
    };
    MyListener listener;
    gcm::Initialize(app, &listener);

    // Poll
    GcmListenerQueue queued_listener;
    gcm::Initialize(app, &queued_listener);
    GcmListenerQueue::Event event;
    while (queued_listener(&event)) {
      switch (event.type) {
        case kEventTypeMessageDeleted:
          // do stuff
          break;
        case kEventTypeMessageReceived:
          // display event.data
          break;
      }
    }
```

# Data Types

* Time: [Milliseconds since the epoch](https://en.wikipedia.org/wiki/Unix_time)

# More on C++ and games

* Performance Oriented C++ for Game Developers (very incomplete).
