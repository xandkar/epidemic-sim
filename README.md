Epidemic Simulator
==================

A work-in-progress exploration of how one could go about simulating an epidemic.
Beginning with a Forest Fire model.

Quick demo: https://www.youtube.com/watch?v=gZ4yIJ1_uvg

TODO
----

### Milestones
- [x] CGOL
- [x] Forest Fire model
- [ ] Agent mobility
- [ ] Quarantine zones
- [ ] Agent attributes
- [ ] Agent destinations (home, )
- [ ] Domain time

### Improvements
- [ ] Aggregates
- [ ] CLI parameters
- [ ] Smart(er) mobility (go to destination rather than wonder randomly)
- [ ] Data export
- [ ] Data plotting
- [ ] Reports
- [ ] Runtime parameters
- [x] Runtime controls from any state: play/pause toggle, forward, back, stop
- [ ] Runtime control pipe:

    echo 'play' > control
    echo 'stop' > control
    echo 'back' > control
    echo 'forw' > control
    echo 'set f 0.001' > control
    echo 'set p 0.025' > control

### Maybe
- [ ] limit generation storage to a circular buffer
- [ ] scripting language beyond basic parameter setting
