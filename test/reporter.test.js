var bindings = require('../')

var realCollector = (!process.env.APPOPTICS_REPORTER
  || process.env.APPOPTICS_REPORTER === 'ssl')
  && (process.env.APPOPTICS_COLLECTOR === 'collector.appoptics.com'
  || process.env.APPOPTICS_COLLECTOR === 'collector-stg.appoptics.com')

describe('addon.reporter', function () {
  var r = new bindings.Reporter()

  it('should initialize oboe with hostnameAlias', function () {
    bindings.oboeInit(process.env.APPOPTICS_SERVICE_KEY, {
      hostnameAlias: 'node-testing-hostname'
    })
  })

  it('should initialize oboe with empty options', function () {
    bindings.oboeInit(process.env.APPOPTICS_SERVICE_KEY, {})
  })

  it('should check if ready to sample', function () {
    // wait 5 seconds max. This will fail if not using
    // a real collector (collector or collector-stg).appoptics.com
    var ready = r.isReadyToSample(5000)
    ready.should.be.an.instanceof(Number)

    if (realCollector) {
      ready.should.equal(1)
    } else {
      ready.should.not.equal(1)
    }
  })

  it('should send a generic span', function () {
    var customName = 'this-is-a-name'
    var domain = 'bruce.com'

    var finalTxName = r.sendNonHttpSpan({
      duration: 1001
    })
    finalTxName.should.equal('unknown')

    finalTxName = r.sendNonHttpSpan({
      domain: domain
    })
    finalTxName.should.equal(domain + '/')

    finalTxName = r.sendNonHttpSpan({
      txname: customName,
      duration: 1111
    })
    finalTxName.should.equal(customName)

    finalTxName = r.sendNonHttpSpan({
      txname: customName,
      domain: domain,
      duration: 1234
    })
    finalTxName.should.equal(domain + '/' + customName)
  })

  it('should send an HTTP span', function () {
    var customName = 'this-is-a-name'
    var domain = 'bruce.com'
    var url = '/api/todo'
    var status = 200
    var method = 'GET'

    var finalTxName = r.sendHttpSpan({
      url: url,
      status: status,
      method: method,
      duration: 1111
    })
    finalTxName.should.equal(url)

    finalTxName = r.sendHttpSpan({
      url: url,
      domain: domain
    })
    finalTxName.should.equal(domain + url)

    finalTxName = r.sendHttpSpan({
      txname: customName,
      url: url,
      duration: 1234
    })
    finalTxName.should.equal(customName)

    finalTxName = r.sendHttpSpan({
      txname: customName,
      url: url,
      domain: domain,
      duration: 1236
    })
    finalTxName.should.equal(domain + '/' + customName)
  })

  it('should not crash node getting the prototype of a reporter instance', function () {
    var p = Object.getPrototypeOf(r)
  })
})
