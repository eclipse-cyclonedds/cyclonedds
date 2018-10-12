# Design documentation

If a feature is sufficiently large or complicated enough, it must be covered
by design documentation. Whether or not something is complicated enough to
require design documentation is not always straightforward and may depend on
target audience, etc. The committer should act as a gatekeeper in this case.

Currently there is no one policy that covers what exactly a good design
document must contain, how it is formatted or what diagrams it must contain
at a minimum. Common sense dictates that it should explain why, rather than
how, be concise and easily accessible. After all, if no one is able to find or
read it, it serves no purpose. There are some guidelines when it comes to
tooling and the directory structure.

capturing and explaining the underlying ideas.


## Documentation file format

As previously stated, the most important part about design documentation is
the fact that people can find it and read it. Therefore, the documentation
should be in a non-propriatary, well supported format. Since most repository
managers, GitHub included, support rendering Markdown on the fly and it is
easy to write and read in any text editor, it seems like a very good choice.

The added benefit of on the fly rendering in GitHub makes that the source is
also the artifact, which allows any contributer to consult up-to-date
design documentation at any time.


## UML Diagrams

Often a clear description of the problem that is addressed by a given
component is enough, but as the saying goes, a picture is worth a thousand
words. There will be scenarios where a UML diagram (e.g. a class diagram,
or a sequence diagram) is a much better way to explain how components are
tied together or a given protocol works.

The arguments for a documentation file format hold up equally well for the
format in which UML models are expressed. There are a number of so-called
textual modeling tools, but [PlantUML](http://plantuml.com/) seems to be the
most popular and supports the most types of diagrams. Strictly speaking,
PlantUML is a tool, not just a format, that generates so-called *dot* files,
which are turned into an image by [Graphviz](https://www.graphviz.org/).
While there are wiki and forum software packages that directly support
rendering PlantUML diagrams, GitHub does (currently) not. Luckily
[Gravizo](http://www.gravizo.com/) supports on the fly rendering of graphs
in PlantUML syntax.

A simple activity diagram describing how a PlantUML model is converted into an
image by Graphviz should be displayed below this text as an example.

![alt text](https://g.gravizo.com/source/g?./diagrams.plantuml)

