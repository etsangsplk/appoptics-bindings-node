#include "bindings.h"

Nan::Persistent<v8::FunctionTemplate> Metadata::constructor;

// currently active
static int metadata_count = 0;
// at destruction - size and average size
static int metadata_deleted_size = 0;
static int metadata_deleted_count = 0;

Metadata::Metadata() {
    metadata_count += 1;
}

// Allow construction of clones
Metadata::Metadata(oboe_metadata_t* md) {
    metadata_count += 1;
    oboe_metadata_copy(&metadata, md);
}

// Remember to cleanup the metadata struct when garbage collected
Metadata::~Metadata() {
    metadata_count -= 1;
    metadata_deleted_size += sizeof(metadata);
    metadata_deleted_count += 1;
    oboe_metadata_destroy(&metadata);
}

bool Metadata::sampleFlagIsOn() {
  return metadata.flags & XTR_FLAGS_SAMPLED;
}

/**
 * JavaScript & C++ callable method to create a new Javascript instance
 *
 *
 */
NAN_METHOD(Metadata::New) {
  if (!info.IsConstructCall()) {
    return Nan::ThrowError("Metadata() must be called as a constructor");
  }

  // Go through the various calling signatures.
  Metadata* md;

  if (info.Length() == 0) {
    // returns null metadata
    md = new Metadata();
  } else if (info.Length() != 1) {
    return Nan::ThrowError("new Metadata() accepts at most 1 parameter");
  } else {
    // there is one argument
    if (info[0]->IsExternal()) {
        // this can only be used by C++ methods, not JavaScript. They must pass
        // the right data - an oboe_metadata_t*.
        oboe_metadata_t* omd = static_cast<oboe_metadata_t*>(info[0].As<v8::External>()->Value());
        md = new Metadata(omd);
    } else {
        // convert Metadata, Event, or String.
        md = Metadata::getMetadata(info[0]);
        if (md == NULL) {
            return Nan::ThrowError("Invalid argument for new Metadata()");
        }
    }
  }

  md->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}


//
// Create a new instance when a Metadata object argument
//
v8::Local<v8::Object> Metadata::NewInstance(Metadata* md) {
  Nan::EscapableHandleScope scope;

  const unsigned argc = 1;
  v8::Local<v8::Value> argv[argc] = { Nan::New<v8::External>(&md->metadata) };
  v8::Local<v8::Function> cons = Nan::New<v8::FunctionTemplate>(constructor)->GetFunction();
  //v8::Local<v8::Object> instance = cons->NewInstance(argc, argv);
  v8::Local<v8::Object> instance = Nan::NewInstance(cons, argc, argv).ToLocalChecked();

  return scope.Escape(instance);
}

//
// Create a new instance when no arguments.
//
v8::Local<v8::Object> Metadata::NewInstance() {
    Nan::EscapableHandleScope scope;

    const unsigned argc = 0;
    v8::Local<v8::Value> argv[argc] = {};
    v8::Local<v8::Function> cons = Nan::New<v8::FunctionTemplate>(constructor)->GetFunction();
    //v8::Local<v8::Object> instance = cons->NewInstance(argc, argv);
    v8::Local<v8::Object> instance = Nan::NewInstance(cons, argc, argv).ToLocalChecked();

    return scope.Escape(instance);
}

NAN_METHOD(Metadata::getMetadataData) {
    v8::Local<v8::String> active = Nan::New<v8::String>("active").ToLocalChecked();
    v8::Local<v8::String> freedBytes = Nan::New<v8::String>("freedBytes").ToLocalChecked();
    v8::Local<v8::String> freedCount = Nan::New<v8::String>("freedCount").ToLocalChecked();

    v8::Local<v8::Object> o = Nan::New<v8::Object>();
    Nan::Set(o, active, Nan::New<v8::Number>(metadata_count));
    Nan::Set(o, freedBytes, Nan::New<v8::Number>(metadata_deleted_size));
    Nan::Set(o, freedCount, Nan::New<v8::Number>(metadata_deleted_count));

    info.GetReturnValue().Set(o);
}

/**
 * JavaScript callable static method to return metadata constructed from the current
 * oboe context.
 */
NAN_METHOD(Metadata::fromContext) {
    Metadata* md = new Metadata(oboe_context_get());
    info.GetReturnValue().Set(Metadata::NewInstance(md));
    delete md;
}

// Transform a string back into a metadata instance
NAN_METHOD(Metadata::fromString) {
  Nan::Utf8String str(info[0]);

  oboe_metadata_t md;
  int status = oboe_metadata_fromstr(&md, *str, str.length());
  // status can be zero with a version other than 2, so check that too.
  if (status < 0 || md.version != 2) {
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }

  Metadata* metadata = new Metadata(&md);
  info.GetReturnValue().Set(Metadata::NewInstance(metadata));
  delete metadata;
}

//
// Metadata factory for randomized metadata
//
NAN_METHOD(Metadata::makeRandom) {
    oboe_metadata_t md;
    oboe_metadata_init(&md);
    oboe_metadata_random(&md);

    // set or clear the sample flag appropriately if an argument specified.
    if (info.Length() == 1) {
        if (info[0]->ToBoolean()->BooleanValue()) {
            md.flags |= XTR_FLAGS_SAMPLED;
        } else {
            md.flags &= ~XTR_FLAGS_SAMPLED;
        }
    }

    // create intermediate metadata so it can be deleted and doesn't leak memory.
    Metadata* metadata = new Metadata(&md);

    // create a metadata object so this parallels the JS invocation.
    info.GetReturnValue().Set(Metadata::NewInstance(metadata));
    delete metadata;
}

// Copy the contents of the metadata instance to a new instance
NAN_METHOD(Metadata::copy) {
    Metadata* self = Nan::ObjectWrap::Unwrap<Metadata>(info.This());
    info.GetReturnValue().Set(Metadata::NewInstance(self));
}

// Verify that the state of the metadata instance is valid
NAN_METHOD(Metadata::isValid) {
    Metadata* self = Nan::ObjectWrap::Unwrap<Metadata>(info.This());
    bool status = oboe_metadata_is_valid(&self->metadata);
    info.GetReturnValue().Set(Nan::New(status));
}

NAN_METHOD(Metadata::getSampleFlag) {
    Metadata* self = Nan::ObjectWrap::Unwrap<Metadata>(info.This());
    bool sampleFlag = self->sampleFlagIsOn();
    info.GetReturnValue().Set(Nan::New(sampleFlag));
}

NAN_METHOD(Metadata::setSampleFlagTo) {
    if (info.Length() != 1) {
        return Nan::ThrowError("setSampleFlagTo requires one argument");
    }

    Metadata* self = Nan::ObjectWrap::Unwrap<Metadata>(info.This());
    bool previous = self->metadata.flags & XTR_FLAGS_SAMPLED;

    // truthy to set it, falsey to clear it
    if (info[0]->ToBoolean()->BooleanValue()) {
        self->metadata.flags |= XTR_FLAGS_SAMPLED;
    } else {
        self->metadata.flags &= ~XTR_FLAGS_SAMPLED;
    }
    info.GetReturnValue().Set(Nan::New(previous));
}

// Serialize a metadata object to a string
NAN_METHOD(Metadata::toString) {
  // Unwrap the Metadata instance from V8
  Metadata* self = Nan::ObjectWrap::Unwrap<Metadata>(info.This());
  char buf[OBOE_MAX_METADATA_PACK_LEN];

  int rc;
  // for now any argument counts. maybe accept 'A' or 'a'?
  if (info.Length() == 1) {
    rc = format(&self->metadata, sizeof(buf), buf) ? 0 : -1;
  } else {
    rc = oboe_metadata_tostr(&self->metadata, buf, sizeof(buf) - 1);
  }

  info.GetReturnValue().Set(Nan::New(rc == 0 ? buf : "").ToLocalChecked());
}

// Create an event from this metadata instance
NAN_METHOD(Metadata::createEvent) {
  Metadata* self = Nan::ObjectWrap::Unwrap<Metadata>(info.This());
  info.GetReturnValue().Set(Event::NewInstance(self));
}

//
// Internal method to determine if an object is an instance of
// JavaScript Metadata
//
bool Metadata::isMetadata(v8::Local<v8::Value> object) {
  return Nan::New(Metadata::constructor)->HasInstance(object);
}

//
// Internal method to return new metadata object if an object
// represents metadata.
//
Metadata* Metadata::getMetadata(v8::Local<v8::Value> object) {
    Metadata* metadata;

    if (Metadata::isMetadata(object)) {
        // it's a metadata instance
        Metadata* md = Nan::ObjectWrap::Unwrap<Metadata>(object->ToObject());
        metadata = new Metadata(&md->metadata);
    } else if (Event::isEvent(object)) {
        // it's an event instance
        Event* e = Nan::ObjectWrap::Unwrap<Event>(object->ToObject());
        metadata = new Metadata(&e->event.metadata);
    } else if (object->IsString()) {
        // it's a string, this can fail and return NULL.
        Nan::Utf8String str(object);

        oboe_metadata_t md;
        int status = oboe_metadata_fromstr(&md, *str, str.length());
        if (status < 0) {
            return NULL;
        }
        metadata = new Metadata(&md);
    } else {
        // it's not something we know how to convert
        return NULL;
    }

    return metadata;
}

// JavaScript callable method to determine if an object is an
// instance of JavaScript Metadata.
NAN_METHOD(Metadata::isInstance) {
    bool is = false;
    if (info.Length() == 1) {
        v8::Local<v8::Value> arg = info[0];
        is = Metadata::isMetadata(arg);
    }
    info.GetReturnValue().Set(is);
}

//
// JavaScript callable static method to determine if the argument
// (event, metadata, or string) has the sample flag turned on.
//
// returns a boolean indicating the state of the sample flag
//
// if a string argument cannot be converted to metadata it
// returns undefined
//
NAN_METHOD(Metadata::sampleFlagIsSet) {
    bool sampleFlag = false;

    // handle each argument type appropriately
    if (info.Length() >= 1) {
        if (Metadata::isMetadata(info[0])) {
            Metadata* md = Nan::ObjectWrap::Unwrap<Metadata>(info[0]->ToObject());
            sampleFlag = md->metadata.flags & XTR_FLAGS_SAMPLED;
        } else if (Event::isEvent(info[0])) {
            Event* e = Nan::ObjectWrap::Unwrap<Event>(info[0]->ToObject());
            sampleFlag = e->event.metadata.flags & XTR_FLAGS_SAMPLED;
        } else if (info[0]->IsString()) {
            Nan::Utf8String str(info[0]);

            oboe_metadata_t md;
            int status = oboe_metadata_fromstr(&md, *str, str.length());
            if (status < 0) {
                info.GetReturnValue().Set(Nan::Undefined());
                return;
            }
            sampleFlag = md.flags & XTR_FLAGS_SAMPLED;
        }
    }

    info.GetReturnValue().Set(Nan::New(sampleFlag));
}

// Wrap the C++ object so V8 can understand it
void Metadata::Init(v8::Local<v8::Object> exports) {
  Nan::HandleScope scope;

  // Prepare constructor template
  v8::Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(Metadata::New);
  constructor.Reset(ctor);
  ctor->InstanceTemplate()->SetInternalFieldCount(2);
  ctor->SetClassName(Nan::New("Metadata").ToLocalChecked());

  // Statics
  Nan::SetMethod(ctor, "getMetadataData", Metadata::getMetadataData);
  Nan::SetMethod(ctor, "fromString", Metadata::fromString);
  Nan::SetMethod(ctor, "fromContext", Metadata::fromContext);
  Nan::SetMethod(ctor, "makeRandom", Metadata::makeRandom);
  Nan::SetMethod(ctor, "isInstance", Metadata::isInstance);
  Nan::SetMethod(ctor, "sampleFlagIsSet", Metadata::sampleFlagIsSet);

  // Prototype
  Nan::SetPrototypeMethod(ctor, "copy", Metadata::copy);
  Nan::SetPrototypeMethod(ctor, "isValid", Metadata::isValid);
  Nan::SetPrototypeMethod(ctor, "getSampleFlag", Metadata::getSampleFlag);
  Nan::SetPrototypeMethod(ctor, "setSampleFlagTo", Metadata::setSampleFlagTo);
  Nan::SetPrototypeMethod(ctor, "toString", Metadata::toString);
  Nan::SetPrototypeMethod(ctor, "createEvent", Metadata::createEvent);

  Nan::Set(exports, Nan::New("Metadata").ToLocalChecked(), ctor->GetFunction());
}

//
// function to format an x-trace with components split by ':'
//
bool Metadata::format(oboe_metadata_t* md, size_t len, char* buffer) {
    char* b = buffer;

    // length counts prefix chars, flag chars, 3 colons and null termination
    if (2 + md->task_len + md->op_len + 2 + 4 > len) return false;

    // fix up the first byte (because it's not 2B in memory)
    int task_bits = (md->task_len >> 2) - 1;
    if (task_bits == 4) task_bits = 3;
    uint8_t packed = md->version << 4 | ((md->op_len >> 2) - 1) << 3 | task_bits;
    b = PutHex(packed, b);
    *b++ = ':';

    // put the task ID
    uint8_t* mdp = (uint8_t *) &md->ids.task_id;
    for(unsigned int i = 0; i < md->task_len; i++) {
        b = PutHex(*mdp++, b);
    }
    *b++ = ':';

    // put the op ID
    mdp = (uint8_t *) &md->ids.op_id;
    for (unsigned int i = 0; i < md->op_len; i++) {
        b = PutHex(*mdp++, b);
    }
    *b++ = ':';

    // put the flags byte and null terminate the string.
    b = PutHex(md->flags, b);
    *b = '\0';

    return true;
}

char* Metadata::PutHex(uint8_t byte, char *p, char base) {
    base -= 10;
    int digit = (byte >> 4);
    digit += digit <= 9 ? '0' : base;
    *p++ = (char) digit;
    digit = byte & 0xF;
    digit += digit <= 9 ? '0' : base;
    *p++ = (char) digit;

    return p;
}
