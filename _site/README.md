#install env:
sudo pacman -S jekyll ruby-bundler

export GEM_HOME=$HOME/github/gem

bundle install  

bundle exec jekyll serve

