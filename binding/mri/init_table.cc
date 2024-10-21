// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/init_table.h"

#include "binding/mri/mri_template.h"
#include "content/public/table.h"

namespace binding {

MRI_DEFINE_DATATYPE_REF(Table, "Table", content::Table);

MRI_METHOD(table_initialize) {
  int x, y = 1, z = 1;
  switch (argc) {
    case 3:
      z = std::max(0, NUM2INT(argv[2]));
    case 2:
      y = std::max(0, NUM2INT(argv[1]));
    case 1:
      x = std::max(0, NUM2INT(argv[0]));
      break;
    default:
      rb_error_arity(argc, 1, 3);
  }

  scoped_refptr<content::Table> obj = new content::Table(x, y, z);
  obj->AddRef();
  MriSetStructData(self, obj.get());

  return self;
}

MRI_METHOD(table_initialize_copy) {
  VALUE other;
  MriParseArgsTo(argc, argv, "o", &other);

  scoped_refptr<content::Table> other_obj =
      MriGetStructData<content::Table>(other);

  scoped_refptr<content::Table> obj = new content::Table(*other_obj);
  obj->AddRef();
  MriSetStructData(self, obj.get());

  return self;
}

MRI_METHOD(table_resize) {
  scoped_refptr<content::Table> obj = MriGetStructData<content::Table>(self);

  int x, y = 1, z = 1;
  switch (argc) {
    case 3:
      z = std::max(0, NUM2INT(argv[2]));
    case 2:
      y = std::max(0, NUM2INT(argv[1]));
    case 1:
      x = std::max(0, NUM2INT(argv[0]));
      break;
    default:
      rb_error_arity(argc, 1, 3);
  }

  obj->Resize(x, y, z);

  return Qnil;
}

MRI_METHOD(table_xsize) {
  scoped_refptr<content::Table> obj = MriGetStructData<content::Table>(self);
  return INT2NUM(obj->GetXSize());
}

MRI_METHOD(table_ysize) {
  scoped_refptr<content::Table> obj = MriGetStructData<content::Table>(self);
  return INT2NUM(obj->GetYSize());
}

MRI_METHOD(table_zsize) {
  scoped_refptr<content::Table> obj = MriGetStructData<content::Table>(self);
  return INT2NUM(obj->GetZSize());
}

MRI_METHOD(table_get_at) {
  scoped_refptr<content::Table> obj = MriGetStructData<content::Table>(self);

  int x = 0, y = 0, z = 0;

  x = NUM2INT(argv[0]);
  if (argc > 1) {
    y = NUM2INT(argv[1]);
  }

  if (argc > 2) {
    z = NUM2INT(argv[2]);
  }

  if (argc > 3)
    MriCheckArgc(argc, 3);

  if (x < 0 || x >= obj->GetXSize() || y < 0 || y >= obj->GetYSize() || z < 0 ||
      z >= obj->GetZSize())
    return Qnil;

  return INT2FIX(obj->Get(x, y, z));
}

MRI_METHOD(table_set_at) {
  scoped_refptr<content::Table> obj = MriGetStructData<content::Table>(self);

  int x = 0, y = 0, z = 0, value;

  if (argc < 2)
    MriCheckArgc(argc, 2);

  switch (argc) {
    default:
    case 2:
      x = NUM2INT(argv[0]);
      value = NUM2INT(argv[1]);
      break;
    case 3:
      x = NUM2INT(argv[0]);
      y = NUM2INT(argv[1]);
      value = NUM2INT(argv[2]);
      break;
    case 4:
      x = NUM2INT(argv[0]);
      y = NUM2INT(argv[1]);
      z = NUM2INT(argv[2]);
      value = NUM2INT(argv[3]);
      break;
  }

  obj->Set(value, x, y, z);

  return argv[argc - 1];
}

void InitTableBinding() {
  VALUE klass = rb_define_class("Table", rb_cObject);
  rb_define_alloc_func(klass, MriClassAllocate<&kTableDataType>);

  MriDefineMethod(klass, "initialize", table_initialize);
  MriDefineMethod(klass, "initialize_copy", table_initialize_copy);
  MriInitSerializableBinding<content::Table>(klass);

  MriDefineMethod(klass, "resize", table_resize);
  MriDefineMethod(klass, "xsize", table_xsize);
  MriDefineMethod(klass, "ysize", table_ysize);
  MriDefineMethod(klass, "zsize", table_zsize);
  MriDefineMethod(klass, "[]", table_get_at);
  MriDefineMethod(klass, "[]=", table_set_at);
}

}  // namespace binding
