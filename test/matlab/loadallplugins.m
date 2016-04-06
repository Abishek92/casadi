import casadi.*

for pl = strsplit(CasadiMeta.getPlugins(),';')
  out  = strsplit(pl{:},'::');
  cls  = out{1};
  name = out{2};
  if strcmp(cls, 'Integrator')
    eval(['load_integrator(''' name ''')'])
  elseif strcmp(cls, 'Nlpsol')
    eval(['load_nlpsol(''' name ''')'])
  elseif strcmp(cls, 'Conic')
    eval(['load_conic(''' name ''')'])
  elseif strcmp(cls, 'Rootfinder')
    eval(['load_rootfinder(''' name ''')'])
  elseif strcmp(cls, 'Linsol')
    eval(['load_linsol(''' name ''')'])
  else
    eval([cls '.loadPlugin(''' name ''')'])
  end


end

