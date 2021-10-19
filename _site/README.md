#install env:
sudo pacman -S ruby
gem install jekyll  
gem install bundler  

export PATH=$PATH:~/.gem/ruby/2.7.0/bin/
bundle install  

to rebuild:
jekyll serve  


for ruby 3.0:
bundle add webrick
