var bindings = require('../')

describe('addon.context', function () {
  it('should set tracing mode to never', function () {
    bindings.Context.setTracingMode(bindings.TRACE_NEVER)
  })
  it('should set tracing mode to always', function () {
    bindings.Context.setTracingMode(bindings.TRACE_ALWAYS)
  })

  it('should throw setting invalid tracing mode value', function () {
    try {
      bindings.Context.setTracingMode(3)
    } catch (e) {
      if (e.message === 'Invalid tracing mode') {
        return
      }
    }

    throw new Error('setTracingMode should fail on invalid inputs')
  })

  it('should throw setting invalid tracing mode type', function () {
    try {
      bindings.Context.setTracingMode('foo')
    } catch (e) {
      if (e.message === 'Tracing mode must be a number') {
        return
      }
    }

    throw new Error('setTracingMode should fail on invalid inputs')
  })

  it('should set valid sample rate', function () {
    bindings.Context.setDefaultSampleRate(bindings.MAX_SAMPLE_RATE / 10)
  })

  it('should not throw when setting invalid sample rate', function () {
    var threw = false
    try {
      bindings.Context.setDefaultSampleRate(-1)
      bindings.Context.setDefaultSampleRate(bindings.MAX_SAMPLE_RATE + 1)
    } catch (e) {
      threw = true
    }

    threw.should.equal(false, 'setting invalid rates threw')
  })

  it('should handle bad sample rates correctly', function () {
    var rateUsed
    rateUsed = bindings.Context.setDefaultSampleRate(-1)
    rateUsed.should.equal(0)
    rateUsed = bindings.Context.setDefaultSampleRate(bindings.MAX_SAMPLE_RATE + 1)
    rateUsed.should.equal(bindings.MAX_SAMPLE_RATE)

    rateUsed = bindings.Context.setDefaultSampleRate(100000)
    rateUsed.should.equal(100000)
    // the C++ code cannot ask oboe what rate was in effect. NaN doesn't not
    // change the value because it cannot be compared, so the addon returns -1.
    // appoptics-apm keeps a local copy of the value and handles this correctly.
    rateUsed = bindings.Context.setDefaultSampleRate(NaN)
    rateUsed.should.equal(-1)
  })

  it('should serialize context to string', function () {
    bindings.Context.clear()
    var string = bindings.Context.toString()
    string.should.equal('2B0000000000000000000000000000000000000000000000000000000000')
  })
  it('should set context to metadata instance', function () {
    var event = bindings.Context.createEvent()
    var metadata = event.getMetadata()
    bindings.Context.set(metadata)
    var v = bindings.Context.toString()
    v.should.not.equal('')
    v.should.equal(metadata.toString())
  })
  it('should set context to metadata instance using JavaScript', function () {
    var md = bindings.Metadata.fromContext()
    var event = new bindings.Event(md)
    bindings.Context.set(event.getMetadata())
    var v = bindings.Context.toString()
    v.should.not.equal('')
    v.should.equal(event.getMetadata().toString())
  })
  it('should set context from metadata string', function () {
    var event = bindings.Context.createEvent()
    var string = event.getMetadata().toString()
    bindings.Context.set(string)
    var v = bindings.Context.toString()
    v.should.not.equal('')
    v.should.equal(string)
  })
  it('should copy context to metadata instance', function () {
    var metadata = bindings.Context.copy()
    var v = bindings.Context.toString()
    v.should.not.equal('')
    v.should.equal(metadata.toString())
  })
  it('should clear the context', function () {
    var string = '2B0000000000000000000000000000000000000000000000000000000000'
    bindings.Context.toString().should.not.equal(string)
    bindings.Context.clear()
    bindings.Context.toString().should.equal(string)
  })

  it('should create an event from the current context', function () {
    var event = bindings.Context.createEvent()
    event.should.be.an.instanceof(bindings.Event)
  })
  it('should start a trace from the current context', function () {
    var event = bindings.Context.startTrace()
    event.should.be.an.instanceof(bindings.Event)
  })

  it('should allow any signature of createEventX', function () {
    var string = '2B0000000000000000000000000000000000000000000000000000000000'
    var md = bindings.Metadata.makeRandom()
    var event = bindings.Context.createEventX()
    event.should.be.an.instanceof(bindings.Event)

    event = bindings.Context.createEventX(string)
    event.should.be.an.instanceof(bindings.Event)

    event = bindings.Context.createEventX(event)
    event.should.be.an.instanceof(bindings.Event)

    event = bindings.Context.createEventX(md)
    event.should.be.an.instanceof(bindings.Event)

    event = bindings.Context.createEventX(md, false)
    event.should.be.an.instanceof(bindings.Event)

    event = bindings.Context.createEventX(md, undefined)
    event.should.be.an.instanceof(bindings.Event)

    event = bindings.Context.createEventX(md, true)
    event.should.be.an.instanceof(bindings.Event)
  })

  it('should get verification that a request should be sampled', function (done) {
    bindings.Context.setTracingMode(bindings.TRACE_ALWAYS)
    bindings.Context.setDefaultSampleRate(bindings.MAX_SAMPLE_RATE)
    var event = bindings.Context.startTrace(1)
    var metadata = event.getMetadata()
    metadata.setSampleFlagTo(1)
    var xid = metadata.toString();
    var counter = 8
    // poll to give time for the SSL connection to complete
    var id = setInterval(function() {
      // requires layer name and string X-Trace ID to check if sampling.
      var check = bindings.Context.sampleTrace('bruce-test', xid)
      if (--counter <= 0 || typeof check === 'object' && check.source !== 2) {
        clearInterval(id)
        check.should.have.property('sample', true)
        check.should.have.property('source', 1)
        check.should.have.property('rate', bindings.MAX_SAMPLE_RATE)
        done()
        return
      }
  }, 500)
  })

  it('should be invalid when empty', function () {
    bindings.Context.clear()
    bindings.Context.isValid().should.equal(false)
  })
  it('should be valid when not empty', function () {
    var event = bindings.Context.startTrace()
    bindings.Context.isValid().should.equal(true)
  })

  it('should not set sample bit unless specified', function () {
    var event = bindings.Context.startTrace()
    event.getSampleFlag().should.equal(false)
    event.toString().slice(-2).should.equal('00')
    event = bindings.Context.startTrace(1)
    event.getSampleFlag().should.equal(true)
    event.toString().slice(-2).should.equal('01')
  })
})
